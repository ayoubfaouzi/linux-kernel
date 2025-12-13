/* view_v2_cgroups.go

   Copyright (C) Michael Kerrisk, 2018

   Licensed under GNU General Public License version 3 or later

   Display one or more subtrees in the cgroups v2 hierarchy. The following
   info is displayed for each cgroup: the cgroup type, the controllers enabled
   in the cgroup, and the process and thread members of the cgroup.
*/

package main

import (
	"bufio"
	"errors"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"syscall"
	"unsafe"
)

// Print a command-line usage message for this program and terminate the
// program with the specified 'status' value.

func showUsageAndExit(status int) {
	fmt.Println(
		`Usage: view_v2_cgroups [options] <cgroup-dir-path>...

Show the state (cgroup type, enabled controllers, member processes, member
TIDs, and, optionally, owning UID) of the cgroups in the cgroup v2
subhierarchies whose pathnames are supplied as the command line arguments.

Options:
--no-color      Don't use color in the displayed output.
--no-pids       Don't show the member PIDs in each cgroup.
--no-tids       Don't show the member TIDs in each cgroup.
--show-owner    Show the user ID of each cgroup.
--show-rt       Highlight realtime threads in the displayed output.
  `)

	os.Exit(status)
}

func main() {
	opts := parseCmdLineOptions()

	if len(flag.Args()) == 0 {
		showUsageAndExit(1)
	}

	for _, path := range flag.Args() {
		displayDirTree(path, opts)
	}
}

// Info from command-line options

type cmdLineOptions struct {
	useColor            bool // Use color in the output
	showPids            bool // Show member PIDs for each cgroup
	showTids            bool // Show member TIDs for each cgroup
	showOwner           bool // Show cgroup ownership
	showRealtimeThreads bool // Highlight realtime threads in the output
}

// Parse command-line options and return them conveniently packaged in a
// structure.

func parseCmdLineOptions() cmdLineOptions {
	var opts cmdLineOptions

	helpPtr := flag.Bool("help", false, "Show detailed usage message")
	noColorPtr := flag.Bool("no-color", false,
		"Don't use color in output display")
	noPidsPtr := flag.Bool("no-pids", false,
		"Don't show PIDs that are members of each cgroup")
	noTidsPtr := flag.Bool("no-tids", false,
		"Don't show TIDs that are members of each cgroup")
	showOwnerPtr := flag.Bool("show-owner", false,
		"Show owner UID for cgroup")
	showRealtimePtr := flag.Bool("show-rt", false,
		"Highlight realtime threads")

	flag.Parse()

	if *helpPtr {
		showUsageAndExit(0)
	}

	opts.useColor = !*noColorPtr
	opts.showPids = !*noPidsPtr
	opts.showTids = !*noTidsPtr
	opts.showOwner = *showOwnerPtr
	opts.showRealtimeThreads = *showRealtimePtr

	return opts
}

func displayDirTree(path string, opts cmdLineOptions) {
	path = filepath.Clean(path) // Remove consecutive + trailing slashes

	// 'rootSlashCnt' is the number of slashes in the pathname of the
	// cgroup that is the root of the subtree that is currently being
	// displayed. This is used for calculating the indent for displaying
	// the descendant cgroups under this root.

	rootSlashCnt := len(strings.Split(path, "/"))

	// Using a closure as the second argument of filepath.Walk(), rather
	// than a separately defined callback function, allows us to pass
	// information (in this case, via 'rootSlashCnt" and 'opts') to the
	// "callback code" executed by filepath.Walk() without the use of
	// global variables.

	err := filepath.Walk(path,
		func(path string, fileInfo os.FileInfo, e error) error {
			if e != nil {
				return e
			}

			if fileInfo.IsDir() { // We're only interested in the cgroup directories
				err := displayCgroup(path, rootSlashCnt, opts)
				if err != nil {
					return err
				}
			}

			return nil
		})

	if err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
}

// Some terminal color sequences for coloring the output.

const ESC = ""
const RED = ESC + "[31m"
const YELLOW = ESC + "[93m"
const BOLD = ESC + "[1m"
const LIGHT_BLUE = ESC + "[38;5;51m"
const GREEN = ESC + "[92m"
const BLUE = ESC + "[34m"
const MAGENTA = ESC + "[35m"
const LIGHT_PURPLE = ESC + "[38;5;93m"
const NORMAL = ESC + "(B" + ESC + "[m"
const GRAY = ESC + "[38;5;240m"
const BRIGHT_YELLOW = ESC + "[93m"
const BLINK = ESC + "[5m"
const REVERSE = ESC + "[7m"
const UNDERLINE = ESC + "[4m"

// A map defining the color used to display the different cgroup types.

var cgroupColor = map[string]string{
	"root":            "",
	"domain":          "",
	"domain threaded": UNDERLINE + BOLD + GREEN,
	"threaded":        GREEN,
	"domain invalid":  REVERSE + LIGHT_PURPLE,
}

// A map defining the string used to display each cgroup type.

var cgroupAbbrev = map[string]string{
	"root":            "[/]",
	"domain":          "[d]",
	"domain threaded": "[dt]",
	"threaded":        "[t]",
	"domain invalid":  "[inv]",
}

// Display all of the info about the cgroup specified by 'cgroupPath'.

func displayCgroup(cgroupPath string, rootSlashCnt int,
	opts cmdLineOptions) (err error) {

	cgroupType := getCgroupType(cgroupPath)

	// Calculate indent according to number of slashes in pathname
	// (relative to the root of the currently displayed subtree).

	level := len(strings.Split(cgroupPath, "/")) - rootSlashCnt
	indent := strings.Repeat(" ", 4*level)

	// At the topmost level, we display the full pathname from the
	// command line. At lower levels, we display just the basename
	// component of the pathname.

	displayPath := cgroupPath
	if level > 0 {
		displayPath = filepath.Base(cgroupPath)
	}

	// We show each cgroup type with a distinctive color/style.

	fmt.Print(indent + cgroupColor[cgroupType] + displayPath + NORMAL +
		" " + cgroupAbbrev[cgroupType])

	err = displayEnabledControllers(cgroupPath, opts)
	if err != nil {
		return err
	}

	fmt.Println()

	if opts.showOwner {
		fmt.Print(indent + "    ")

		err = displayCgroupOwnership(cgroupPath, opts)
		if err != nil {
			return err
		}

		fmt.Println()
	}

	err = displayCgroupMembers(cgroupPath, cgroupType, indent+"    ", opts)
	if err != nil {
		return err
	}

	return nil
}

// Return the type of a cgroup (taken from the cgroup.type file).

func getCgroupType(cgroupPath string) (cgroupType string) {
	path := cgroupPath + "/" + "cgroup.type"

	ct, err := os.ReadFile(path)
	if err != nil {
		// The most likely reason for failure is that the 'cgroup.type'
		// file does not exist because this is the root cgroup.
		if os.IsNotExist(err) {
			cgroupType = "root"
		} else { // Unexpected error
			fmt.Println("Could not read from ", path)
			os.Exit(1)
		}
	} else {
		cgroupType = strings.TrimSpace(string(ct))
	}

	return cgroupType
}

// Display the controllers that are enabled for the cgroup specified by
// 'cgroupPath'.

func displayEnabledControllers(cgroupPath string, opts cmdLineOptions) error {
	scPath := cgroupPath + "/" + "cgroup.subtree_control"
	sc, err := os.ReadFile(scPath)
	if err != nil {
		return err
	}

	controllers := strings.TrimSpace(string(sc)) // Trim trailing newline

	if controllers != "" {
		controllers = "(" + controllers + ")"
		if opts.useColor {
			controllers = BRIGHT_YELLOW + controllers + NORMAL
		}
		fmt.Print("    " + controllers)
	}

	return nil
}

// Display the ownership of a cgroup directory.

func displayCgroupOwnership(cgroupPath string, opts cmdLineOptions) error {
	fileInfo, err := os.Stat(cgroupPath)
	if err != nil {
		return err
	}

	stat, ok := fileInfo.Sys().(*syscall.Stat_t)
	if !ok {
		return errors.New("fileInfo.Sys() failure for " + cgroupPath)
		return err
	}

	if opts.useColor {
		fmt.Print(MAGENTA)
	}

	fmt.Print("<UID: " + strconv.Itoa(int(stat.Uid)))
	//fmt.Print("; GID: " + strconv.Itoa(int(stat.Gid)))
	//fmt.Print("; " + fmt.Sprint(fileInfo.Mode())[1:])
	fmt.Print(">")

	if opts.useColor {
		fmt.Print(NORMAL)
	}

	return nil
}

// Display the member processes and member threads of the cgroup specified by
// 'cgroupPath'.

func displayCgroupMembers(cgroupPath string, cgroupType string,
	indent string, opts cmdLineOptions) error {

	// Calculate display width of PID and TID lists.

	const minDisplayWidth = 32
	displayWidth := getTerminalWidth() - len(indent)
	if displayWidth < minDisplayWidth {
		displayWidth = minDisplayWidth
	}

	// The 'cgroup.procs' file is not readable in "threaded" cgroups.

	if cgroupType != "threaded" && opts.showPids {
		err := displayMemberProcesses(cgroupPath, displayWidth, indent,
			opts)
		if err != nil {
			return err
		}
	}

	if opts.showTids {
		err := displayMemberThreads(cgroupPath, displayWidth, indent,
			opts)
		if err != nil {
			return err
		}
	}

	return nil
}

// Discover width of terminal, so that we can format output suitably.

func getTerminalWidth() int {
	type winsize struct {
		row    uint16
		col    uint16
		xpixel uint16
		ypixel uint16
	}
	var ws winsize

	ret, _, _ := syscall.Syscall(syscall.SYS_IOCTL,
		uintptr(syscall.Stdout), uintptr(syscall.TIOCGWINSZ),
		uintptr(unsafe.Pointer(&ws)))

	if int(ret) == -1 { // Call failed (perhaps stdout is not a terminal)
		return 80
	}

	return int(ws.col)
}

// Display the set of processes that are members of the cgroup 'cgroupPath'.

func displayMemberProcesses(cgroupPath string, displayWidth int,
	indent string, opts cmdLineOptions) error {

	pids, err := getSortedIntsFrom(cgroupPath + "/" + "cgroup.procs")
	if err != nil {
		return err
	}

	if len(pids) > 0 {
		buf := strconv.Itoa(pids[0])
		for _, p := range pids[1:] {
			buf += " " + strconv.Itoa(p)
		}

		buf = wrapText(buf+"}", "PIDs: {", displayWidth, indent)

		if opts.useColor {
			buf = colorEachLine(buf, LIGHT_BLUE)
		}

		fmt.Println(buf)
	}

	return nil
}

// Display the set of threads that are members of the cgroup 'cgroupPath'.

func displayMemberThreads(cgroupPath string, displayWidth int,
	indent string, opts cmdLineOptions) error {

	tidLIst, err := getSortedIntsFrom(cgroupPath + "/" + "cgroup.threads")
	if err != nil {
		return err
	}

	if len(tidLIst) == 0 {
		return nil
	}

	buf := ""
	for i, tid := range tidLIst {
		if i > 0 {
			buf += " "
		}

		buf += fmt.Sprint(tid)

		if opts.showRealtimeThreads {
			buf += realtimeThreadMarking(tid, opts)
		}

		tgid, err := getTgid(tid)
		if err != nil {
			continue // Probably, thread already terminated
		}

		if tgid != tid {
			buf += "-[" + fmt.Sprint(tgid) + "]"
		}
	}

	buf = wrapText(buf+"}", "TIDs: {", displayWidth, indent)

	if opts.useColor {
		buf = colorEachLine(buf, LIGHT_BLUE)
	}

	fmt.Println(buf)

	return nil
}

// Determine whether the thread 'tid' is scheduled under a realtime scheduling
// policy. We do this because in older kernels (before Linux 5.4) the cgroups
// v2 'cpu' controller didn't understand realtime threads. Consequently, on
// such kernels realtime threads must be placed in the root cgroup before the
// 'cpu' controller can be enabled. In order to highlight the presence of
// realtime threads in nonroot cgroups, we display these threads with a
// distinctive marker.

func realtimeThreadMarking(tid int, opts cmdLineOptions) string {
	isRealtime, err := isRealtimeThread(tid)
	if err != nil {
		return "" // Probably, thread already terminated
	}

	marking := ""

	if isRealtime {
		const RT_THREAD_MARKER = "*"

		if opts.useColor {
			marking += RED + REVERSE
		}
		marking += RT_THREAD_MARKER
		if opts.useColor {
			marking += NORMAL + LIGHT_BLUE
		}
	}

	return marking
}

// Return a flag indicating whether the thread with the specified TID is
// scheduled under a realtime policy.

func isRealtimeThread(tid int) (bool, error) {
	const SCHED_FIFO = 1
	const SCHED_RR = 2
	const SCHED_DEADLINE = 6

	type sched_param struct {
		sched_priority uint32
	}

	var sp sched_param
	var policy int

	ret, _, err := syscall.Syscall6(syscall.SYS_SCHED_GETSCHEDULER,
		uintptr(tid), uintptr(unsafe.Pointer(&sp)),
		uintptr(0), uintptr(0), uintptr(0), uintptr(0))

	policy = int(ret)

	if policy == -1 {
		return false, err
	}

	isRealtime := policy == SCHED_DEADLINE || policy == SCHED_FIFO ||
		policy == SCHED_RR

	return isRealtime, nil
}

// Obtain the thread group ID (PID) of the thread 'tid' by looking up the
// appropriate field in the /proc/TID/status file.

func getTgid(tid int) (int, error) {
	statusFile := "/proc/" + strconv.Itoa(tid) + "/status"

	status, err := os.Open(statusFile)
	if err != nil {

		// Probably, the thread terminated between the time we
		// accessed the namespace files and the time we tried to open
		// /proc/TID/status.

		return 0, err
	}

	defer status.Close() // Close file on return from this function.

	// Find the line containing the 'Tgid:' entry.

	re := regexp.MustCompile(":[ \t]*")

	scanner := bufio.NewScanner(status)
	for scanner.Scan() {
		match, _ := regexp.MatchString("^Tgid:", scanner.Text())
		if match {
			tokens := re.Split(scanner.Text(), -1)
			tgid, _ := strconv.Atoi(tokens[1])
			return tgid, nil
		}
	}

	// There should always be a 'Tgid:' entry, but just in case there
	// is not...

	err = errors.New("Error scanning )" + statusFile +
		": could not find 'Tgid' field")
	return 0, err
}

// Read the contents of 'path', which should be a file containing white-space
// delimited integers, and return those integers as a sorted slice.

func getSortedIntsFrom(path string) ([]int, error) {
	buf, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}

	if len(buf) == 0 {
		return nil, nil
	}

	var list []int
	for _, s := range strings.Split(strings.TrimSpace(string(buf)), "\n") {
		i, _ := strconv.Atoi(s)
		list = append(list, i)
	}

	sort.Ints(list)

	return list, nil
}

// Return wrapped version of text in 'text' by adding newline characters on
// white space boundaries at most 'displayWidth' characters apart. Each wrapped
// line is prefixed by the specified 'indent' (whose size is *not* included as
// part of 'displayWidth' for the purpose of the wrapping algorithm).  The
// first line of output is additionally prefixed by the string in 'prefix', and
// subsequent lines are also additionally prefixed by an equal amount of white
// space.

func wrapText(text string, prefix string, displayWidth int,
	indent string) string {

	// Break up text on white space to produce a slice of words.

	words := strings.Fields(text)

	if len(words) == 0 { // No words! ==> return an empty string.
		return ""
	}

	wrappedText := indent + prefix + words[0]
	column := len(prefix) + displayLength(words[0])
	displayWidth -= len(prefix)
	indent += strings.Repeat(" ", len(prefix))

	for _, word := range words[1:] {
		wordLen := displayLength(word)
		if column+wordLen+1 > displayWidth { // Start on new line
			wrappedText += "\n" + indent + word
			column = wordLen
		} else {
			wrappedText += " " + word
			column += 1 + wordLen
		}
	}

	return wrappedText
}

// Calculate displayed length of a string. This length excludes any
// escape sequences used for coloring.

func displayLength(word string) int {
	re := regexp.MustCompile(ESC + "[^m]*m")
	s := re.ReplaceAllString(word, "")
	return len(s)
}

// Put a terminal color sequence just before the first non-white-space
// character in each line of 'buf', and place the terminal sequence to return
// the terminal color to white at the end of each line.

func colorEachLine(buf string, color string) string {
	re := regexp.MustCompile(`( *)(.*)`)
	return re.ReplaceAllString(buf, "$1"+color+"$2"+NORMAL)
}
