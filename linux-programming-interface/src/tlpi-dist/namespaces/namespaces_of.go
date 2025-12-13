/* namespaces_of.go

   Copyright (C) Michael Kerrisk, 2018-2024

   Licensed under GNU General Public License version 3 or later

   Show the namespace memberships of one or more processes in the context
   of the user or PID namespace hierarchy.

   This program does one of the following:
   * If provided with a list of PIDs, this program shows the namespace
     memberships of those processes.
   * If no PIDs are provided, the program shows the namespace memberships
     of all processes on the system (which it discovers by parsing the
     /proc/PID/ns directories).
   * If no PIDs are provided, and the "--subtree=<pid>" option is specified,
     then the program shows the subtree of the PID or user namespace hierarchy
     that is rooted at the namespace of the specified PID.

   By default, the program shows namespace memberships in the context of the
   user namespace hierarchy, showing also the nonuser namespaces owned by each
   user namespace. If the "--pidns" option is specified, the program instead
   shows just the PID namespace hierarchy.

   The "--no-pids" option suppresses the display of the processes that are
   members of each namespace.

   The "--show-comm" option displays the command being run by each process.

   The "--all-pids" option can be used in conjunction with "--pidns", so that
   for each process that is displayed, its PIDs in all of the PID namespaces of
   which it is a member are shown.

   The "--no-color" option can be used to suppress the use of color in the
   displayed output.

   When displaying the user namespace hierarchy, the "--namespaces=<list>"
   option can be used to specify a list of the nonuser namespace types to
   include in the displayed output; the default is to include all nonuser
   namespace types.

   This program discovers the namespaces on the system, and their
   relationships, by scanning /proc/PID/ns/* symlink files and matching the
   device IDs and inode numbers of those files using the operations described
   in ioctl_ns(2). In cases where the program must inspect symlink files of
   processes that are owned by other users, the program must be run as
   superuser.

   The following rules imposed by the kernel mean that all of the threads in a
   multithreaded process must be in the same user and PID namespaces:

   * When calling clone(2), the CLONE_THREAD flag can't be specified in
     conjunction with either CLONE_NEWUSER or CLONE_NEWPID.
   * A multithreaded process can't call unshare(CLONE_NEWUSER) or use setns()
     to change its user namespace
   * unshare(CLONE_NEWPID) and setns() with an argument that specifies a PID
     namespace do not change the PID namespace of the caller.

   Therefore, it is not necessary to scan the /proc/PID/task/TID/ns directories
   to discover any further information about the shape of the user or PID
   namespace hierarchy. However, scanning /proc/PID/task/TID/ns can uncover
   information about other types of namespaces that are occupied only by
   noninitial threads (and this fact is the motivation for the "--search-tasks"
   option).
*/

package main

import (
	"bufio"
	"flag"
	"fmt"
	"os"
	"regexp"
	"runtime"
	"sort"
	"strconv"
	"strings"
	"syscall"
	"unsafe"
)

// showUsageAndExit() prints a command-line usage message for this program and
// terminates the program with the specified 'status' value.

func showUsageAndExit(status int) {
	fmt.Println(
		`Usage: namespaces_of [options] [--subtree=<pid> | <pid>...]

Show the namespace memberships of one or more processes in the context of the
user or PID namespace hierarchy.

This program does one of the following:
* If provided with one or more PID command-line arguments, the program shows
  the namespace memberships of those processes.
* Otherwise, if the "--subtree=<pid>" option is specified, then the program
  shows the subtree of the user or PID namespace hierarchy that is rooted at
  the namespace of the specified PID.
* Otherwise, the program shows the namespace memberships of all processes on
  the system.

By default, the program shows namespace memberships in the context of the user
namespace hierarchy, showing also the nonuser namespaces owned by each user
namespace. If the "--pidns" option is specified, the program shows only the
PID namespace hierarchy, omitting other types of namespace.

Options:

--all-pids	For each displayed process, show PIDs in all namespaces of
		which the process is a member (used only in conjunction with
		"--pidns").
--deep-scan	Also show namespaces pinned into existence for reasons other
		than having member processes, being an owning user namespace,
		or being an ancestor (user or PID) namespace. This includes
		namespaces that are pinned into existence by bind mounts, by
		open file descriptors, and by 'pid_for_children' or
		'time_for_children' symlinks.
--namespaces=<list>
		Show just the listed namespace types when displaying the
		user namespace hierarchy. <list> is a comma-separated list
		containing one or more of "cgroup", "ipc", "mnt", "net", "pid",
		"time", "user", and "uts". (The default is to include all
		nonuser namespace types in the display of the user namespace
		hierarchy.) To see just the user namespace hierarchy, use
		"--namespaces=user".
--no-color	Suppress the use of color in the displayed output.
--no-pids	Suppress the display of the processes that are members of each
		namespace.
--pidns		Display the PID namespace hierarchy (rather than the user
		namespace hierarchy).
--search-tasks	Look for namespaces via /proc/PID/task/*/ns/* rather than
		/proc/PID/ns/*. (Does more work in order to find namespaces
		that may be occupied by noninitial threads.) Also causes
		member TIDs (rather than PIDs) to be displayed for each
		namespace.
--show-comm	Displays the command being run by each process.

Syntax notes:
* No PID command-line arguments may be supplied when using "--deep-scan",
  "--search-tasks", or "--subtree=<pid>".
* At most one of "--namespaces" and "--pidns" may be specified.
* "--all-pids" can be specified only in conjunction with "--pidns".
* "--no-pids" can't be specified in conjunction with either "--show-comm"
  "--all-pids".`)

	os.Exit(status)
}

func init() {
	// Ensure that no goroutine runs in same OS thread as main().
	runtime.LockOSThread()
}

func main() {
	nsi := namespaceInfo{nsList: make(namespaceList)}

	opts := parseCmdLineOptions()

	// Determine which namespace symlink files are to be processed. (By
	// default, all namespaces are processed, but this can be changed via
	// command-line options.)

	nsSymlinks := namespaceSymlinkNames
	if opts.showPidnsHierarchy {
		nsSymlinks = []string{"pid"}
	}

	// Add namespace entries for specified processes.

	if len(flag.Args()) == 0 || opts.subtreePID != "" {
		nsi.addNamespacesForAllProcesses(nsSymlinks, opts)

		// If we scanned all processes on the system (i.e., no PID
		// command-line arguments were supplied), then we probably have
		// at least one PID in each user namespace. This enables us to
		// discover the UID and GID map for each user namespace, so do
		// that discovery in order that we can display the maps. We
		// don't do this if only some of the PIDs on the system are
		// scanned, since then it's likely that we gathered information
		// about some user namespaces without discovering any of their
		// member processes.

		if len(flag.Args()) == 0 {
			nsi.addUidGidPMaps()
		}
	} else {
		// Add namespaces for PIDs named in the command-line arguments.
		// (flag.Args() is the set of command-line words that were not
		// options.)

		for _, pid := range flag.Args() {
			for _, nsFile := range nsSymlinks {
				nsi.addProcessNamespace(pid, nsFile, opts, true)
			}
		}
	}

	// Display the results of the namespace scan.

	nsi.displayNamespaceHierarchies(opts)
}

// The following structure stores info from command-line options.

type cmdLineOptions struct {
	useColor           bool   // Use color in the output
	searchTasks        bool   // Search for namespaces via /proc/PID/task/*
	showCommand        bool   // Show the command being run by each process
	noPids             bool   // Don't show member PIDs for each namespace
	showAllPids        bool   // Show all of a process's PIDs (PID NS only)
	showPidnsHierarchy bool   // Display the PID namespace hierarchy
	deepScan           bool   // Do extra work to discover other namespaces
	subtreePID         string // Display hierarchy rooted at specific PID
	namespaces         int    // Bit mask of CLONE_NEW* values
}

// parseCmdLineOptions() parses command-line options and returns them
// conveniently packaged in a structure.

func parseCmdLineOptions() (opts cmdLineOptions) {
	helpPtr := flag.Bool("help", false, "Show detailed usage message")
	noColorPtr := flag.Bool("no-color", false,
		"Don't use color in output display")
	noPidsPtr := flag.Bool("no-pids", false,
		"Don't show PIDs that are members of each namespace")
	searchTasksPtr := flag.Bool("search-tasks", false,
		"Search for namespaces vi /proc/PID/task/*")
	showCommandPtr := flag.Bool("show-comm", false,
		"Show command run by each PID")
	deepScanPtr := flag.Bool("deep-scan", false,
		"Also show namespaces that are pinned into existence for "+
			"less usual reasons")
	allPidsPtr := flag.Bool("all-pids", false,
		"Show all PIDs of each process")
	pidnsPtr := flag.Bool("pidns", false, "Show PID "+
		"namespace hierarchy (instead of user namespace hierarchy)")
	subtreePtr := flag.String("subtree", "", "Show namespace subtree "+
		"rooted at namespace of specified process")
	namespacesPtr := flag.String("namespaces", "", "Show just the "+
		"specified namespaces")

	flag.Parse()

	opts.useColor = !*noColorPtr
	opts.searchTasks = *searchTasksPtr
	opts.noPids = *noPidsPtr
	opts.showPidnsHierarchy = *pidnsPtr
	opts.showCommand = *showCommandPtr
	opts.showAllPids = *allPidsPtr
	opts.deepScan = *deepScanPtr
	opts.subtreePID = *subtreePtr

	if *helpPtr {
		showUsageAndExit(0)
	}

	if len(flag.Args()) > 0 &&
		(opts.deepScan || opts.searchTasks || opts.subtreePID != "") {
		fmt.Println("No PID arguments may specified in combination " +
			"with \"--deep-scan\", " +
			"\"--search-tasks\", or \"--subtree=<pid>\"")
		showUsageAndExit(1)
	}

	if *namespacesPtr != "" && opts.showPidnsHierarchy {
		fmt.Println("\"--namespaces=<list>\" can't be specified " +
			"with \"--pidns\"")
		showUsageAndExit(1)
	}

	if opts.showAllPids && !opts.showPidnsHierarchy {
		fmt.Println("\"--all-pids\" can be specified only with " +
			"\"--pidns\"")
		showUsageAndExit(1)
	}

	if opts.noPids && (opts.showCommand || opts.showAllPids) {
		fmt.Println("\"--no-pids\" can't be combined with " +
			"\"--show-comm\" or \"--all-pids\"")
		showUsageAndExit(1)
	}

	// Calculate the opts.namespaces bit mask, which determines which
	// namespaces we will display. The bit mask will be some subset of the
	// CLONE_NEW* flags.

	opts.namespaces = getNamespaceFlags(*namespacesPtr)

	return opts
}

// Calculate which namespaces to display, based on the "--namespaces"
// command-line option (or all namespaces by default.)

func getNamespaceFlags(nsNames string) int {
	namespaces := namespaceSymlinkNames // Default is all available namespaces

	if nsNames != "" {
		namespaces = strings.Split(nsNames, ",")
	}

	// Parse list of namespaces to display, by tokenizing 'namespaces' on
	// comma delimiters, finding each token string in 'namespaceMap', and
	// adding corresponding key (a CLONE_NEW* value) to 'opts.namespaces'.

	flags := 0

	for _, nsName := range namespaces {
		nsFlag := 0
		for k, v := range namespaceMap {
			if v == nsName {
				nsFlag = k
				break
			}
		}

		if nsFlag == 0 {
			fmt.Println("Bad name in \"--namespaces\" option: " +
				nsName)
			showUsageAndExit(1)
		}

		flags |= nsFlag
	}

	return flags
}

// A namespace is uniquely identified by the combination of a device ID
// and an inode number.

type namespaceID struct {
	device uint64 // dev_t
	inode  uint64 // ino_t
}

// For each namespace, we record a number of attributes, beginning with the
// namespace type and the PIDs of the processes that are members of the
// namespace. In the case of user namespaces, we also record (a) the nonuser
// namespaces that the namespace owns and (b) the user namespaces that are
// parented by this namespace; alternatively, if the "--pidns" option was
// specified, we record just the namespaces that are parented by this
// namespace. Finally, for user namespaces, we record the UID of the namespace
// creator.

type namespace struct {
	nsType     int           // CLONE_NEW*
	pids       []int         // Member processes
	children   []namespaceID // Child+owned namespaces (user/PID NSs only)
	creatorUID int           // UID of creator (user NSs only)
	uidMap     string        // UID map (user NSs only)
	gidMap     string        // UID map (user NSs only)
}

type namespaceList map[namespaceID]*namespace

// The 'namespaceInfo' structure records information about the namespaces
// that have been discovered:
// * The 'nsList' map records all of the namespaces that we visit.
// * While adding the first namespace to 'nsList', we'll discover the ancestor
//   of all namespaces (the root of the user or PID namespace hierarchy).
//   We record the ID of that namespace in 'rootNS'.
// * We may encounter nonuser namespaces whose user namespace owners are not
//   visible because they are ancestors of the user's user namespace (i.e.,
//   this program is being run from a noninitial user namespace, in a shell
//   started by a command such as "unshare -Uripf --mount-proc"). We record
//   these namespaces as being children of a special entry in the 'nsList' map,
//   with the key 'invisUserNS'. (The implementation of this special entry
//   presumes that there is no namespace file that has device ID 0 and inode
//   number 0.)

type namespaceInfo struct {
	nsList namespaceList
	rootNS namespaceID
}

var invisUserNS = namespaceID{0, 0} // Const value

// Namespace ioctl() operations (see ioctl_ns(2)).

const NS_GET_USERNS = 0xb701    // Get owning user NS (or parent of user NS)
const NS_GET_PARENT = 0xb702    // Get parent NS (for user or PID NS)
const NS_GET_NSTYPE = 0xb703    // Return namespace type (see below)
const NS_GET_OWNER_UID = 0xb704 // Return creator UID for user NS

// Namespace types returned by NS_GET_NSTYPE.

const CLONE_NEWNS = 0x00020000
const CLONE_NEWCGROUP = 0x02000000
const CLONE_NEWUTS = 0x04000000
const CLONE_NEWIPC = 0x08000000
const CLONE_NEWUSER = 0x10000000
const CLONE_NEWPID = 0x20000000
const CLONE_NEWNET = 0x40000000
const CLONE_NEWTIME = 0x00000080

// 'namespaceMap' lists all known namespaces, mapping the CLONE_* constant name
// to the corresponding symlink filename in /proc/PID/ns.

var namespaceMap = map[int]string{
	CLONE_NEWCGROUP: "cgroup",
	CLONE_NEWIPC:    "ipc",
	CLONE_NEWNS:     "mnt",
	CLONE_NEWNET:    "net",
	CLONE_NEWPID:    "pid",
	CLONE_NEWTIME:   "time",
	CLONE_NEWUSER:   "user",
	CLONE_NEWUTS:    "uts",
}

// 'namespaceMap' was initialized with a list of all of the currently known
// namespace symlink names. However, we might be running on an older kernel
// that does not know about some newer namespaces (e.g., time namespaces). So,
// we build a list of the namespace symlinks that are actually available on
// this system by checking the names in 'namespaceMap' against what exists in
// /proc/self/ns.

func getNamespaceSymlinkNames() (names []string) {
	for _, nsFile := range namespaceMap {
		if _, err := os.Stat("/proc/self/ns/" + nsFile); err == nil {
			names = append(names, nsFile)
		}
	}

	sort.Strings(names)
	return names
}

// The set of namespace symlink files in the /proc/PID/ns directory that
// actually exist on this running kernel.

var namespaceSymlinkNames []string = getNamespaceSymlinkNames()

// addNamespacesForAllProcesses() scans /proc/PID directories to build
// namespace entries in 'nsi' for all processes on the system.

func (nsi *namespaceInfo) addNamespacesForAllProcesses(namespaces []string,
	opts cmdLineOptions) {

	// Fetch a list of the filenames under /proc.

	procFiles, err := os.ReadDir("/proc")
	if err != nil {
		fmt.Println("os.ReadDir():", err)
		os.Exit(1)
	}

	// Process each /proc/PID (PID starts with a digit).

	for _, file := range procFiles {
		if file.Name()[0] >= '1' && file.Name()[0] <= '9' {
			nsi.addNamespacesForOneProcess(namespaces,
				file.Name(), opts)
		}
	}
}

// Add namespaces for one process, or all of the threads of that process.
//
// Note that searching for namespaces via /proc/PID/task/*/ns/* really can find
// namespaces that would not be found by scanning just /proc/PID/ns/* symlinks.
// Consider a program that does the following:
//
//      Main thread creates second thread with pthread_create()
//              Second thread does unshare(CLONE_NEWUTS)
//              Second thread sleeps
//      Main thread terminates
//
// In the above, a /proc/PID entry (/proc/main-TGID) still exists for the main
// thread and is visible under /proc. A /proc/PID entry (/proc/thread-2-TID)
// also exists for thread 2, but is not visible under /proc; however, the
// thread is visible as /proc/main-PID/task/thread-2-TID.
//
// In this scenario, most of the symlinks in /proc/main-TGID/ns/ are not
// readable.  However, the symlinks in /proc/main-TGID/task/thread-2-TID/ns/
// (and likewise /proc/thread-2-TID/ns) are readable, and it is only by
// examining those symlinks that we would discover the UTS namespace in which
// thread 2 is the only member.
//
// (For reason that are not obvious, the /proc/main-TGID/ns/{pid,user} symlinks
// *are* still readable even after the main thread has terminated; perhaps it
// is not a coincidence that those are the only two hierarchical namespace
// types; or perhaps it is connected to the fact that we can't combine
// CLONE_THREAD with either of CLONE_NEWUSER or CLONE_NEWPID when calling
// clone(2).)

func (nsi *namespaceInfo) addNamespacesForOneProcess(namespaces []string,
	pid string, opts cmdLineOptions) {

	if !opts.searchTasks {
		// Just use /proc/PID entry.
		nsi.addTheNamespaces(namespaces, pid, opts)
	} else {
		// Fetch a list of the thread IDs under /proc/PID/task.

		tidFiles, err := os.ReadDir("/proc/" + pid + "/task")
		if err != nil {
			// Perhaps the process terminated already. Skip it.
			return
		}

		for _, tidFile := range tidFiles {
			nsi.addTheNamespaces(namespaces, tidFile.Name(), opts)
		}
	}
}

// Add namespaces for the process/thread with the specified 'id'. In the case
// of threads this "just works (TM)" because, for every (noninitial) thread, a
// /proc/<TID> directory exists even though it is not visible if we ls(1) on
// /proc.

func (nsi *namespaceInfo) addTheNamespaces(namespaces []string, id string,
	opts cmdLineOptions) {

	for _, nsFile := range namespaces {
		nsi.addProcessNamespace(id, nsFile, opts, false)
	}
	if opts.deepScan {
		nsi.addPinnedNamespaces(id, opts)
	}
}

// addProcessNamespace() processes a single /proc/PID/ns/* entry, creating a
// namespace entry for that file and, as necessary, namespace entries for all
// ancestor namespaces going back to the initial namespace. 'pid' is a string
// containing a PID; 'nsFile' is a string identifying which namespace symlink
// to open.

func (nsi *namespaceInfo) addProcessNamespace(pid string, nsFile string,
	opts cmdLineOptions, isCmdLineArg bool) {

	// Obtain a file descriptor that refers to the namespace corresponding
	// to 'pid' and 'nsFile'.

	nsfd, err := syscall.Open("/proc/"+pid+"/ns/"+nsFile, syscall.O_RDONLY, 0)

	if nsfd < 0 {
		fmt.Print("Could not open " + "/proc/" + pid + "/ns/" +
			nsFile + ": ")

		if err == syscall.EACCES {
			// We didn't have permission to open /proc/PID/ns/*.

			fmt.Println(err)
			fmt.Println("Rerun this program as superuser")
			os.Exit(1)
		} else {
			// The most likely other error is ENOENT ("no such
			// file"). We differentiate two cases when dealing with
			// the error: the specified PID came from the command
			// line or it is one of a list produced by scanning
			// /proc/PID. In the first case, we assume that the
			// user supplied an invalid PID, diagnose an error and
			// terminate. In the second case, it may be that a
			// /proc/PID entry disappeared from under our
			// feet--that is, the process terminated while we were
			// parsing /proc. If this happens, we simply print a
			// message and carry on.

			if isCmdLineArg {
				fmt.Println(err)
				os.Exit(1)
			} else {
				fmt.Println("Process " + pid +
					" terminated while we were parsing?")
				return
			}
		}
	}

	// Add entry for this namespace, and all of its ancestor namespaces.

	npid, _ := strconv.Atoi(pid)
	nsi.addNamespace(nsfd, npid, opts)

	syscall.Close(nsfd)
}

// addNamespace() adds the namespace referred to by the file descriptor 'nsfd'
// to the 'nsi.nsList' map (creating an entry in the map if one does not
// already exist) and, if 'pid' is greater than zero, adds the PID it contains
// to the list of PIDs that are resident in the namespace.
//
// This function is recursive (via the helper function addNamespaceToList()),
// calling itself to ensure that an entry is also created for the parent or
// owning namespace of the namespace referred to by 'nsfd'. Once that has been
// done, the namespace referred to by 'nsfd' is made a child of the
// parent/owning namespace. Note that, except in the case of the initial
// namespace, a parent/owning namespace must exist, since it is pinned into
// existence by the existence of the child/owned namespace (and that namespace
// is in turn pinned into existence by the open file descriptor 'nsfd').
//
// 'pid' is a PID to be added to the list of PIDs resident in this namespace.
// When called recursively to create the ancestor namespace entries, this
// function is called with 'pid' as -1, meaning that no PID needs to be added
// for this namespace entry.
//
// The return value of the function is the ID of the namespace entry (i.e., the
// device ID and inode number corresponding to the namespace file referred to
// by 'nsfd').

func (nsi *namespaceInfo) addNamespace(nsfd int, pid int,
	opts cmdLineOptions) namespaceID {

	nsid := newNamespaceID(nsfd)

	// If this namespace is not already in the namespaces list of 'nsi',
	// add it to the list.

	if _, fnd := nsi.nsList[nsid]; !fnd {
		nsi.addNamespaceToList(nsid, nsfd, opts)
	}

	// Add PID to PID list for this namespace entry.

	if pid > 0 {
		nsi.nsList[nsid].pids = append(nsi.nsList[nsid].pids, pid)
	}

	return nsid
}

// Create and return a new namespace ID using the device ID and inode number of
// the namespace referred to by 'nsfd'.

func newNamespaceID(nsfd int) namespaceID {
	var sb syscall.Stat_t
	err := syscall.Fstat(nsfd, &sb)
	if err != nil {
		fmt.Println("syscall.Fstat():", err)
		os.Exit(1)
	}

	return namespaceID{sb.Dev, sb.Ino}
}

// addNamespaceToList() adds the namespace 'nsid' to the namespaces list of
// 'nsi'. For an explanation of the remaining arguments, see the comments for
// addNamespace().

func (nsi *namespaceInfo) addNamespaceToList(nsid namespaceID, nsfd int,
	opts cmdLineOptions) {

	// Namespace entry does not yet exist in 'nsList' map; create it.

	nsi.nsList[nsid] = new(namespace)
	nsi.nsList[nsid].nsType, _ = namespaceType(nsfd, true)

	// If this is a user namespace, record the user ID of the creator.

	if nsi.nsList[nsid].nsType == CLONE_NEWUSER {
		nsi.nsList[nsid].creatorUID = getCreatorUID(nsfd)
	}

	// Get a file descriptor for the parent/owning namespace.
	// NS_GET_USERNS returns the owning user namespace when its argument is
	// a nonuser namespace, and (conveniently) returns the parent user
	// namespace when its argument is a user namespace. On the other hand,
	// if we are handling only the PID namespace hierarchy, then we must
	// use NS_GET_PARENT to get the parent PID namespace.

	ioctlOp := NS_GET_USERNS
	if opts.showPidnsHierarchy {
		ioctlOp = NS_GET_PARENT
	}

	parentFD, err := ioctlGet(nsfd, ioctlOp)

	if parentFD == -1 {

		// Any error other than EPERM is unexpected; bail.

		if err != syscall.EPERM {
			fmt.Println("ioctl(parent/owner):", err)
			os.Exit(1)
		}

		// We got an EPERM error...

		if nsi.nsList[nsid].nsType == CLONE_NEWUSER ||
			ioctlOp == NS_GET_PARENT {

			// If the current namespace is a user namespace and
			// NS_GET_USERNS fails with EPERM, or we are processing
			// only PID namespaces and NS_GET_PARENT fails with
			// EPERM, then this is the root namespace (or, at
			// least, the topmost visible namespace); remember it.

			nsi.rootNS = nsid
		} else {
			// Otherwise, we are inspecting a nonuser namespace and
			// NS_GET_USERNS failed with EPERM, meaning that the
			// user namespace that owns this nonuser namespace is
			// not visible (i.e., is an ancestor user namespace).
			// Record these namespaces as children of a special
			// entry in the 'nsList' map. (For an example, use:
			// sudo unshare -Ur sh -c 'go run namespaces_of.go $$')

			if _, fnd := nsi.nsList[invisUserNS]; !fnd {

				// The special parent entry does not yet exist;
				// create it.

				nsi.nsList[invisUserNS] = new(namespace)
				nsi.nsList[invisUserNS].nsType = CLONE_NEWUSER
			}

			nsi.nsList[invisUserNS].children =
				append(nsi.nsList[invisUserNS].children, nsid)
		}
	} else {
		// The ioctl() operation successfully returned a parent/owning
		// namespace; make sure that namespace has an entry in the map.
		// Specify the 'pid' argument as -1, meaning that there is no
		// PID to be recorded as being a member of the parent/owning
		// namespace.

		parent := nsi.addNamespace(parentFD, -1, opts)

		// Make the current namespace entry a child of the
		// parent/owning namespace entry.

		nsi.nsList[parent].children =
			append(nsi.nsList[parent].children, nsid)

		syscall.Close(parentFD)
	}
}

// namespaceType() returns a CLONE_NEW* constant telling us what kind of
// namespace is referred to by 'nsfd'.

func namespaceType(nsfd int, failOnErr bool) (int, error) {
	nsType, err := ioctlGet(nsfd, NS_GET_NSTYPE)

	if nsType == -1 && failOnErr {
		fmt.Println("ioctl(NS_GET_NSTYPE)", err)
		os.Exit(1)
	}

	return nsType, err
}

// Wrapper for ioctl() call that returns an integer.

func ioctlGet(fd int, op int) (int, error) {
	retp, _, err := syscall.Syscall(syscall.SYS_IOCTL,
		uintptr(fd), uintptr(op), 0)
	ret := (int)((uintptr)(unsafe.Pointer(retp)))
	return ret, err
}

// Return the UID of the creator of the user namespace referred to by 'nsfd'.

func getCreatorUID(nsfd int) (uid int) {
	ret, _, err := syscall.Syscall(syscall.SYS_IOCTL,
		uintptr(nsfd), uintptr(NS_GET_OWNER_UID),
		uintptr(unsafe.Pointer(&uid)))

	if (int)((uintptr)(unsafe.Pointer(ret))) == -1 {
		fmt.Println("ioctl(NS_GET_OWNER_UID):", err)
		os.Exit(1)
	}

	return uid
}

// Add UID and GID maps for all of the user namespaces in 'nsi'.

func (nsi *namespaceInfo) addUidGidPMaps() {
	for _, ns := range nsi.nsList {
		if ns.nsType == CLONE_NEWUSER {
			ns.uidMap = findMap(ns.pids, "uid_map")
			ns.gidMap = findMap(ns.pids, "gid_map")
		}
	}
}

// Walk through the list of PIDs in the namespace until we can successfully
// read the contents of a /proc/PID/[ug]id_map file. (We try all PIDs in the
// list until one succeeds because some PIDs may have terminated already.)

func findMap(pids []int, mapType string) string {
	idMap := "deleted"
	for _, pid := range pids {
		fnd, val := readMap(pid, mapType)
		if fnd {
			idMap = val
			break
		}
	}

	return idMap
}

// Read the contents of the UID or GID map of the process with the specified
// 'pid'. 'mapName' is either "uid_map" or "gid_map". The returned string
// contains the map with white space compressed.

func readMap(pid int, mapName string) (bool, string) {
	mapPath := "/proc/" + strconv.Itoa(pid) + "/" + mapName

	mapBytes, err := os.ReadFile(mapPath)
	if err != nil {
		// Probably, the process terminated between the time we
		// accessed the namespace files and the time we tried to open
		// the map file.

		return false, "deleted"
	}

	mapString := strings.TrimSpace(string(mapBytes)) // Trim trailing "\n"
	space := regexp.MustCompile(`\s+`)
	return true, space.ReplaceAllString(mapString, " ")
}

// By default, this program discovers only the namespaces that are visible via
// /proc/PID/ns/* files. This means that if a namespace does not contain any
// processes, this program will discover the namespace only if it is the owning
// user namespace of a namespace that has member processes, or it is the
// ancestor user or PID namespace of a namespace that is otherwise discovered.
//
// However, there are other reasons why a namespace might be pinned into
// existence even though it has no member processes:
//
// (1) there is a 'time_for_children' or 'pid_for_children' symlink that refers
//     to the namespace.
// (2) there is an open file descriptor that refers to the namespace; or
// (3) there is a bind mount that refers to the namespace.
//
// Namespaces for case (1) can be discovered simply by scanning the symlinks.
//
// Namespaces for case (2) can be discovered by examining /proc/PID/fd/*
// entries for each process to find file descriptors that refer to namespaces.
//
// Namespaces for case (3) can be discovered by parsing /proc/PID/mountinfo
// entries for each process (after first stepping into the processes's mount
// namespace) to discover any bind mounts that refer to namespaces.
//
// addPinnedNamespaces() does the extra work required to discover namespaces
// via the above three means.

var auxNamespaceSymlinks = map[int]string{
	CLONE_NEWPID:  "pid_for_children",
	CLONE_NEWTIME: "time_for_children",
}

func (nsi *namespaceInfo) addPinnedNamespaces(pid string, opts cmdLineOptions) {
	for nsType, nsFile := range auxNamespaceSymlinks {

		// When displaying only PID namespace hierarchy, we add entries
		// only for symlinks that refer to PID namespaces.

		if !opts.showPidnsHierarchy || nsType == CLONE_NEWPID {
			nsi.addNamespaceForSymlink(pid, nsFile, opts)
		}
	}

	nsi.addNamespacesForProcPidFds(pid, opts)

	nsi.addNamespacesForBindMounts(pid, opts)
}

// Add a namespace referred to by a /proc/PID/fd/*_for_children symlink.

func (nsi *namespaceInfo) addNamespaceForSymlink(pid, nsFile string,
	opts cmdLineOptions) {

	// Obtain a file descriptor that refers to the namespace
	// corresponding to 'pid' and 'nsFile'.

	nsfd, _ := syscall.Open("/proc/"+pid+"/ns/"+nsFile, syscall.O_RDONLY, 0)

	if nsfd < 0 {
		// On error, just abandon. Some errors are expected. For
		// example a 'pid_for_children' symlink won't be openable if no
		// processes have been created in that PID namespace. Or the
		// link may have disappeared because the process has terminated.
		// Or the link might not be provided in this kernel version
		// (e.g., "time_for_children" appeared only in Linux 5.6).

		return
	}

	// Add entry for this namespace, and all of its ancestor namespaces.

	nsi.addNamespace(nsfd, -1, opts)

	syscall.Close(nsfd)
}

// Add any namespaces referred to by the file descriptors of 'pid'. We discover
// those file descriptors using the /proc/PID/fd/* symlinks.

func (nsi *namespaceInfo) addNamespacesForProcPidFds(pid string,
	opts cmdLineOptions) {

	// Fetch a list of the symlinks under /proc/PID/fd.

	fdFiles, err := os.ReadDir("/proc/" + pid + "/fd")
	if err != nil {
		// Perhaps the process terminated already. Skip it.

		return
	}

	// Process each /proc/PID/fd/* symlink. We open the symlink in order to
	// obtain a file descriptor that refers to the object that the symlink
	// points to. In the case where the symlink refers to a namespace, this
	// gives us a file descriptor that refers to the namespace.

	for _, file := range fdFiles {
		// O_NONBLOCK because opens of some types of file
		// (e.g., certain types of devices) can block.
		fd, _ := syscall.Open("/proc/"+pid+"/fd/"+file.Name(),
			syscall.O_RDONLY|syscall.O_NONBLOCK, 0)

		if fd < 0 {
			// Various kinds of (non-namespace) files can't be
			// opened. We don't care about those files.

			continue
		}

		nsType, _ := namespaceType(fd, false)

		if nsType != -1 { // This FD refers to a namespace...

			// When displaying only PID namespace hierarchy, we add
			// entries only for FDs that refer to PID namespaces.

			if !opts.showPidnsHierarchy || nsType == CLONE_NEWPID {
				nsi.addNamespace(fd, -1, opts)
			}
		}

		syscall.Close(fd)
	}
}

// Add the namespaces for the 'nsfs' bind mounts that are visible in the
// /proc/PID/mountinfo of 'pid'.

func (nsi *namespaceInfo) addNamespacesForBindMounts(pid string,
	opts cmdLineOptions) {

	// We could just call processBindMounts() directly, since it scans
	// /proc/PID/mountinfo looking for 'nsfs' bind mounts. However, each
	// call to processBindMounts() results in the creation of a new OS
	// thread, which is expensive. On the assumption that few processes
	// have 'nsfs' bind mounts, it's cheaper to do a precheck of
	// /proc/PID/mountinfo in order to decide whether we need to call
	// processBindMounts().

	if mountinfoHasNsfsEntries(pid) {

		// We do the real work in a goroutine. We do things this way,
		// so that the work can be done in a separate OS thread that is
		// able to step into the mount namespace of 'pid'.

		finished := make(chan bool)
		go nsi.processBindMounts(pid, finished, opts)
		<-finished
	}
}

// Return true if the /proc/PID/mountinfo of 'pid' contains any 'nsfs'
// ("namespace filesystem") bind mounts.

func mountinfoHasNsfsEntries(pid string) bool {
	mountinfo, err := os.Open("/proc/" + pid + "/mountinfo")
	if err != nil {
		// Probably, the process terminated between the time we
		// accessed the namespace files and the time we tried to open
		// /proc/PID/mountinfo. Ignore this process.

		return false
	}

	defer mountinfo.Close() // Close file on return from this function.

	miScanner := bufio.NewScanner(mountinfo)
	for miScanner.Scan() {
		match, _ := regexp.MatchString(".*nsfs.*", miScanner.Text())
		if match {
			return true
		}
	}
	return false
}

// Do the real work to add the namespaces for the 'nsfs' bind mounts that are
// visible in the /proc/PID/mountinfo of 'pid'. This function is called as a
// goroutine, and places itself into a separate OS thread.

func (nsi *namespaceInfo) processBindMounts(pid string, finished chan bool,
	opts cmdLineOptions) {

	// Call runtime.LockOSThread() to force this goroutine to use a
	// separate OS thread. This is necessary since we are about to change
	// mount namespace membership and this should not change the namespace
	// membership of main().

	runtime.LockOSThread()

	defer func() { finished <- true }() // Close channel on function return

	ok := stepIntoMountNamespaceOf(pid)
	if !ok {
		return
	}

	nsi.addNamespacesFromMountinfo(pid, opts)
}

// Use setns() to step into the mount namespace of 'pid'.
//
// Returns true if the setns() step was successful, or false otherwise.

func stepIntoMountNamespaceOf(pid string) bool {

	// Unshare filesystem attributes. If these are being shared, then
	// setns() into the mount namespace will fail with EINVAL.

	err := syscall.Unshare(syscall.CLONE_FS)
	if err != nil {
		fmt.Println("unshare failed: ", err)
		os.Exit(1)
	}

	// Obtain a file descriptor that refers to the mount namespace of 'pid'.

	mntnsPath := "/proc/" + pid + "/ns/mnt"
	mntns, err := syscall.Open(mntnsPath, syscall.O_RDONLY, 0)
	if err != nil {
		// Perhaps the process already terminated. In that case, there
		// is nothing for us to do.

		fmt.Println("Open failed: ", mntnsPath, ": ", err)
		return false
	}

	defer syscall.Close(mntns) // Close file on return from this function.

	// There is no 'syscall' method for setns(), since the combination of
	// the golang runtime, underlying OS threads, and rules about the
	// setns() call make would make it difficult to get the use of a
	// syscall.Setns() right. See for example:
	//
	// https://github.com/vishvananda/netns/issues/17
	// https://stackoverflow.com/questions/25704661/calling-setns-from-go-returns-einval-for-mnt-namespace
	// https://www.weave.works/blog/linux-namespaces-and-go-don-t-mix
	// https://groups.google.com/g/golang-dev/c/6G4rq0DCKfo
	// http://thediveo.github.io/#/art/namspill
	//
	// Lacking a 'syscall' method, we use RawSysCall() instead.  And
	// hopefully we've done all the right things to ensure that setns()
	// can correctly do its job.

	const SYS_SETNS = 308
	res, _, msg := syscall.RawSyscall(SYS_SETNS, uintptr(mntns), 0, 0)
	if res != 0 {
		fmt.Println("setns() on "+mntnsPath+" namespace failed:", msg)
		os.Exit(1)
	}

	return true
}

// Add the namespaces for the 'nsfs' bind mounts in the proc/PID/mountinfo of
// 'pid'.

func (nsi *namespaceInfo) addNamespacesFromMountinfo(pid string,
	opts cmdLineOptions) {

	path := "/proc/thread-self/mountinfo"
	mountinfo, err := os.Open(path)
	if err != nil {
		// Probably, the process terminated between the time we
		// accessed the namespace files and the time we tried to open
		// /proc/PID/mountinfo. Ignore this process.

		return
	}

	defer mountinfo.Close() // Close file on return from this function.

	miScanner := bufio.NewScanner(mountinfo)
	for miScanner.Scan() {
		match, _ := regexp.MatchString(".*nsfs.*", miScanner.Text())
		if match {
			// Field 4 is the pathname of the bind mount.
			pathname := strings.Split(miScanner.Text(), " ")[4]
			/*
				fmt.Println("addNamespacesFromMountinfo(): MATCH: " +
					pathname)
			*/
			// Open the pathname, in order to obtain a file
			// descriptor that refers to the corresponding
			// namespace.

			nsfd, err := syscall.Open(pathname, syscall.O_RDONLY, 0)
			if err != nil {
				// Maybe the bind mount was removed after we
				// opened the /proc/PID/mountinfo file. In that
				// case, we skip it.

				continue
			}

			// When displaying only PID namespace hierarchy, we add
			// entries only for bind mounts that refer to PID
			// namespaces.

			nsType, _ := namespaceType(nsfd, false)

			if !opts.showPidnsHierarchy || nsType == CLONE_NEWPID {
				// Add entry for this namespace, and all of its
				// ancestor namespaces.
				nsi.addNamespace(nsfd, -1, opts)
			}

			syscall.Close(nsfd)
		}
	}
}

// Define some terminal escape sequences for displaying color output.

const ESC = ""
const RED = ESC + "[31m"
const YELLOW = ESC + "[93m"
const BOLD = ESC + "[1m"
const LIGHT_BLUE = ESC + "[38;5;51m"
const NORMAL = ESC + "(B" + ESC + "[m"
const PID_COLOR = LIGHT_BLUE
const USERNS_COLOR = YELLOW + BOLD

// displayNamespaceHierarchies() displays the namespace hierarchy/hierarchies
// specified by the command-line options.

func (nsi *namespaceInfo) displayNamespaceHierarchies(opts cmdLineOptions) {
	if opts.subtreePID == "" {
		// No "--subtree" option was specified; display the namespace
		// tree rooted at the initial namespace.

		nsi.displayNamespaceTree(nsi.rootNS, 0, opts)

		// Display the namespaces owned by (invisible) ancestor user
		// namespaces.

		if _, fnd := nsi.nsList[invisUserNS]; fnd {
			nsi.displayNamespaceTree(invisUserNS, 0, opts)
		}
	} else {
		// Display subtree of the namespace hierarchy rooted at the
		// namespace of the PID specified in the "--subtree" option.

		nsFile := "user"
		if opts.showPidnsHierarchy {
			nsFile = "pid"
		}

		nsfd := openNamespaceSymlink(opts.subtreePID, nsFile)

		nsi.displayNamespaceTree(newNamespaceID(nsfd), 0, opts)

		syscall.Close(nsfd)
	}
}

// openNamespaceSymlink() opens a user or PID namespace symlink (specified in
// 'nsFile') for the process with the specified 'pid' and returns the resulting
// file descriptor.

func openNamespaceSymlink(pid string, nsFile string) int {
	symlinkPath := "/proc/" + pid + "/ns/" + nsFile

	nsfd, err := syscall.Open(symlinkPath, syscall.O_RDONLY, 0)
	if nsfd < 0 {
		fmt.Println("Error finding namespace subtree for PID"+
			pid+":", err)
		os.Exit(1)
	}

	return nsfd
}

// displayNamespaceTree() recursively displays the namespace subtree inside
// 'nsi.nsList' that is rooted at 'nsid'.

func (nsi *namespaceInfo) displayNamespaceTree(nsid namespaceID, level int,
	opts cmdLineOptions) {

	// Display 'nsid' if its type is one of those specified in
	// 'opts.namespaces', but always display user namespaces.

	if nsi.nsList[nsid].nsType == CLONE_NEWUSER ||
		nsi.nsList[nsid].nsType&opts.namespaces != 0 {

		nsi.displayNamespace(nsid, level, opts)
	}

	// Recursively display the child namespaces.

	for _, child := range nsi.nsList[nsid].children {
		nsi.displayNamespaceTree(child, level+1, opts)
	}
}

// Display the namespace node with the key 'nsid'. 'level' is our current level
// in the tree, and is used to produce suitably indented output.

func (nsi *namespaceInfo) displayNamespace(nsid namespaceID, level int,
	opts cmdLineOptions) {

	indent := strings.Repeat(" ", level*4)

	// Display the namespace type and ID (device ID + inode number).

	colorUserNS := nsi.nsList[nsid].nsType == CLONE_NEWUSER && opts.useColor

	if colorUserNS {
		fmt.Print(USERNS_COLOR)
	}

	if nsid == invisUserNS {
		fmt.Println("[invisible ancestor user NS]")
	} else {
		fmt.Print(indent+namespaceMap[nsi.nsList[nsid].nsType]+" ", nsid)

		// For user namespaces, display creator UID.

		if nsi.nsList[nsid].nsType == CLONE_NEWUSER {
			fmt.Print(" <UID: ", nsi.nsList[nsid].creatorUID)
			if len(flag.Args()) == 0 {
				fmt.Print(";  ")
				fmt.Print("u: ", nsi.nsList[nsid].uidMap, ";   ")
				fmt.Print("g: ", nsi.nsList[nsid].gidMap)
			}
			fmt.Print(">")
		}

		fmt.Println()
	}

	if colorUserNS {
		fmt.Print(NORMAL)
	}

	// Optionally display member PIDs for the namespace.

	if !opts.noPids {
		displayMemberPIDs(indent, nsi.nsList[nsid].pids, opts)
	}
}

// Print a sorted list of the PIDs that are members of a namespace.

func displayMemberPIDs(indent string, pids []int, opts cmdLineOptions) {

	// If the namespace has no member PIDs, there's nothing to do. (This
	// could happen if a parent namespace has no member processes, but has
	// a child namespace that has a member process.)

	if len(pids) == 0 {
		return
	}

	sort.Ints(pids)

	if opts.showCommand || opts.showAllPids {
		displayPIDsOnePerLine(indent, pids, opts)
	} else {
		displayPIDsAsList(indent, pids, opts)
	}
}

// displayPIDsOnePerLine() prints 'pids' in sorted order, one per line,
// optionally with the name of the command being run by the process. This
// function is called because either 'opts.showCommand' or 'opts.showAllPids'
// was true.

func displayPIDsOnePerLine(indent string, pids []int, opts cmdLineOptions) {
	for _, pid := range pids {
		fmt.Print(indent + strings.Repeat(" ", 8))

		// If the "--show-all-pids" option was specified (which means
		// that "--pidns" must also have been specified), then print
		// all of the process's PIDs in each of the PID namespaces of
		// which it is a member. Otherwise, print the PID in the
		// current PID namespace.

		if opts.showAllPids {
			printAllPIDsFor(pid, opts)

			if !opts.showCommand {
				fmt.Println()
			}
		} else { // 'opts.showCommand' must be true
			if opts.useColor {
				fmt.Print(PID_COLOR)
			}
			fmt.Printf("%-5d", pid)
			if opts.useColor {
				fmt.Print(NORMAL)
			}
		}

		if opts.showCommand { // Print command being run by the process.
			commFile := "/proc/" + strconv.Itoa(pid) + "/comm"

			comm, err := os.ReadFile(commFile)
			if err != nil {
				// Probably, the process terminated between the
				// time we accessed the namespace files and the
				// time we tried to open /proc/PID/comm. We print
				// a diagnostic message and continue.

				fmt.Println("[can't open " + commFile + "]")
			} else {
				fmt.Print("  " + string(comm))
			}
		}
	}
}

// printAllPIDsFor() looks up the 'NStgid' field in the /proc/PID/status file
// of 'pid' and displays the set of PIDs contained in that field.

func printAllPIDsFor(pid int, opts cmdLineOptions) {
	path := "/proc/" + strconv.Itoa(pid) + "/status"
	status, err := os.Open(path)
	if err != nil {
		// Probably, the process terminated between the time we
		// accessed the namespace files and the time we tried to open
		// /proc/PID/status. We print a diagnostic message and keep
		// going.

		fmt.Print("[can't open " + path + "]")
		return
	}

	defer status.Close() // Close file on return from this function.

	// Scan file line by line, looking for 'NStgid:' entry (not the
	// misnamed 'NSpid' field!), and print the corresponding set of PIDs.

	re := regexp.MustCompile(":[ \t]*")

	statusScanner := bufio.NewScanner(status)
	for statusScanner.Scan() {
		match, _ := regexp.MatchString("^NStgid:", statusScanner.Text())
		if match {
			tokens := re.Split(statusScanner.Text(), -1)

			if opts.useColor {
				fmt.Print(PID_COLOR)
			}
			fmt.Print("{ ", tokens[1], " }")
			if opts.useColor {
				fmt.Print(NORMAL)
			}

			break
		}
	}
}

// displayPIDsAsList() prints the PIDs in 'pids' as a sorted list, with
// multiple PIDs per line. We produce a list of PIDs that is suitably wrapped
// and indented, rather than a long single-line list. The output is targeted
// for the terminal width, but even when deeply indenting, a minimum number of
// characters is displayed on each line.

func displayPIDsAsList(indent string, pids []int, opts cmdLineOptions) {

	// Even if deeply indenting, always display at least 'minDisplayWidth'
	// characters on each line.

	const minDisplayWidth = 32

	totalIndent := indent + strings.Repeat(" ", 8)

	outputWidth := getTerminalWidth() - len(totalIndent)
	if outputWidth < minDisplayWidth {
		outputWidth = minDisplayWidth
	}

	// Convert slice of ints to a string of space-delimited words.

	outputBuf := "[ " + strconv.Itoa(pids[0])
	for _, pid := range pids[1:] {
		outputBuf += " " + strconv.Itoa(pid)
	}
	outputBuf += " ]"

	outputBuf = wrapText(outputBuf, outputWidth, totalIndent)
	if opts.useColor {
		outputBuf = colorEachLine(outputBuf, PID_COLOR)
	}

	fmt.Println(outputBuf)
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

// Return wrapped version of text in 'text' by adding newline characters on
// white space boundaries at most 'width' characters apart. Each wrapped line
// is prefixed by the specified 'indent' (whose size is *not* included as part
// of 'width' for the purpose of the wrapping algorithm).

func wrapText(text string, width int, indent string) string {

	// Break up text on white space to produce a slice of words.

	words := strings.Fields(text)

	if len(words) == 0 { // No words!
		return ""
	}

	result := indent + words[0]
	column := len(words[0])

	for _, word := range words[1:] {
		if column+len(word)+1 > width {
			// Overflow ==> start on new line
			result += "\n" + indent + word
			column = len(word)
		} else {
			result += " " + word
			column += 1 + len(word)
		}
	}

	return result
}

// colorEachLine() puts a terminal color sequence just before the first
// non-white-space character in each line of 'buf', and places the terminal
// sequence to return the terminal color to white at the end of each line.

func colorEachLine(buf string, color string) string {
	re := regexp.MustCompile(`( *)(.*)`)
	return re.ReplaceAllString(buf, "$1"+color+"$2"+NORMAL)
}
