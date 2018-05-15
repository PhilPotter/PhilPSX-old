/*
 * This C file models the CD of the PlayStation as a class.
 * 
 * CD.c - Copyright Phillip Potter, 2018
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include "../headers/ArrayList.h"
#include "../headers/LinkedList.h"
#include "../headers/CD.h"

// List of track types
#define PHILPSX_TRACKTYPE_AUDIO 0
#define PHILPSX_TRACKTYPE_MODE2_2352 1

// Path separator for portability
static const char pathSeparator = 
#ifdef _WIN32
'\\';
#else
'/';
#endif

// Forward declarations for functions and subcomponents private to this class
// CD-related stuff:
static void CD_trimLinkedList(LinkedList *list);
static bool CD_readLineToList(LinkedList *list, int fileDescriptor);
static bool CD_getTimesFromLine(char *line, int *minutes, int *seconds,
		int *frames);
static bool CD_swapState(CD *cd, int cdFileDescriptor, size_t cdFileSize,
		int8_t *cdMapping, ArrayList *trackList);
static void CD_convertListToString(LinkedList *list, char *dst);
static bool CD_openBin(const char *path, int *cdFileDescriptor,
		size_t *cdFileSize, int8_t **cdMapping);
static bool CD_addTrackToList(ArrayList *trackList, int32_t type,
		int64_t prevEnd, int64_t trackStart, int64_t gap);
static bool CD_handleUtf8Bom(int cueFileDescriptor);

// CDTrack-related stuff:
typedef struct CDTrack CDTrack;
static CDTrack *construct_CDTrack(int32_t trackNumber, int32_t type,
		int64_t trackStart, int64_t offset);
static void destruct_CDTrack(CDTrack *cdt);
static void destruct_CDTrack_Passthrough(void *cdt);
static bool CDTrack_isWithin(CDTrack *cdt, int64_t position);

/*
 * This struct models a CD itself, and abstracts image type away from the
 * emulator, allowing different image types to be supported.
 */
struct CD {
	
	// This stores the file descriptor for the CD image
	int cdFileDescriptor;

	// This stores the size of the CD image
	size_t cdFileSize;
	
	// This stores the mapping to access the file's contents
	int8_t *cdMapping;
	
	// This allows us to keep a list of tracks from the image
	ArrayList *trackList;
};

/*
 * This struct models a track on the CD.
 */
struct CDTrack {
	
	// Track properties
	int32_t trackNumber;
	int32_t type;
	int64_t trackStart;
	int64_t trackEnd;
	int64_t offset;
};

/*
 * This function constructs an empty CD object with no image bound.
 */
CD *construct_CD(void)
{
	// Allocate CD struct
	CD *cd = malloc(sizeof(CD));
	if (!cd) {
		fprintf(stderr, "PhilPSX: CD: Couldn't allocate memory for "
				"CD struct\n");
		goto end;
	}
	
	// Allocate trackList ArrayList object
	cd->trackList =
		construct_ArrayList(&destruct_CDTrack_Passthrough, false, true);
	if (!cd->trackList) {
		fprintf(stderr, "PhilPSX: CD: Couldn't allocate ArrayList to store"
				" track information\n");
		goto cleanup_cd;
	}
	
	// Set other properties
	cd->cdFileDescriptor = 0;
	cd->cdFileSize = 0;
	cd->cdMapping = NULL;
	
	// Normal return:
	return cd;
	
	// Cleanup path:
	cleanup_cd:
	free(cd);
	cd = NULL;
	
	end:
	return cd;
}

/*
 * This function destructs a CD object.
 */
void destruct_CD(CD *cd)
{	
	// Unmap CD file
	if (munmap(cd->cdMapping, cd->cdFileSize) != 0) {
		fprintf(stderr, "PhilPSX: CD: Couldn't unmap previous file mapping "
				"from address space in destructor\n");
	}

	// Close file
	if (close(cd->cdFileDescriptor) != 0) {
		fprintf(stderr, "PhilPSX: CD: Couldn't close previous CD file in "
				"destructor\n");
	}
	
	// Destruct trackList array list
	destruct_ArrayList(cd->trackList);
	
	// Free CD itself
	free(cd);
}

/*
 * This function tells us if the CD object is currently associated with a
 * loaded image file.
 */
bool CD_isEmpty(CD *cd)
{
	return cd->cdMapping == NULL;
}

/*
 * This functions opens a cue file specified by cdPath and then maps it via
 * the specified CD object.
 */
bool CD_loadCD(CD *cd, const char *cdPath)
{
	// Define various needed values
	bool retVal = true;
	size_t pathLength = strlen(cdPath);
	int cueFileDescriptor = -1, cdFileDescriptor = -1;
	LinkedList *cueLineAsList = NULL, *binPath = NULL;
	ArrayList *trackList = NULL;
	int8_t *cdMapping = NULL;
	char *finalPath = NULL;

	// Check path isn't empty or not long enough
	if (pathLength == 0) {
		fprintf(stderr, "PhilPSX: CD: Provided CD path was empty\n");
		goto cleanup_resources;
	}
	else if (pathLength < 5) {
		fprintf(stderr, "PhilPSX: CD: Provided CD path was too short "
				"and therefore cannot be a cue file of the form "
				"x.cue or x.CUE\n");
		goto cleanup_resources;
	}
	
	// Check if the specified file is a cue file
	const char *fileEnding = cdPath + pathLength - 4;
	if (!strstr(fileEnding, ".cue") && !strstr(fileEnding, ".CUE")) {
		fprintf(stderr, "PhilPSX: CD: Provided CD path was not a cue file\n");
		goto cleanup_resources;
	}

	// Display path
	fprintf(stdout, "PhilPSX: CD: Cue file path is %s\n", cdPath);

	// Open the cue file
	cueFileDescriptor = open(cdPath, O_RDONLY);
	if (cueFileDescriptor == -1) {
		fprintf(stderr, "PhilPSX: CD: Couldn't open cue file\n");
		goto cleanup_resources;
	}

	// Check if CUE file contains UTF-8 BOM, and advance past it if so
	if (!CD_handleUtf8Bom(cueFileDescriptor)) {
		fprintf(stderr, "PhilPSX: CD: Couldn't execute UTF-8 BOM routine\n");
		goto cleanup_resources;
	}

	// Parse the binary file path
	// Read line in one character at a time
	cueLineAsList = construct_LinkedList(NULL, true, true);
	if (!cueLineAsList) {
		fprintf(stderr, "PhilPSX: CD: Couldn't allocate linked list to store "
				"lines of cue file as we parse them\n");
		goto cleanup_resources;
	}
	binPath = construct_LinkedList(NULL, true, true);
	if (!binPath) {
		fprintf(stderr, "PhilPSX: CD: Couldn't allocate linked list to store "
				"path from cue file to binary CD image file\n");
		goto cleanup_resources;
	}

	// Construct new ArrayList to store paths
	trackList = construct_ArrayList(&destruct_CDTrack_Passthrough, false, true);
	if (!trackList) {
		fprintf(stderr, "PhilPSX: CD: Couldn't allocate second ArrayList to "
				"store track information\n");
		goto cleanup_resources;
	}

	// Keep reading until we get to end of line or file, searching for
	// FILE header
	bool binFound = false;
	while (!binFound) {
		LinkedList_wipeAllObjects(cueLineAsList);

		// Read line from cue file
		if (!CD_readLineToList(cueLineAsList, cueFileDescriptor)) {
			fprintf(stderr, "PhilPSX: CD: Failed to read a line to the "
					"linked list\n");
			goto cleanup_resources;
		}

		// We have the line from the cue file, now lets trim it
		CD_trimLinkedList(cueLineAsList);

		// Convert line to C-style string to make use of standard
		// library functions
		char cueLine[LinkedList_getSize(cueLineAsList) + 1];
		CD_convertListToString(cueLineAsList, cueLine);

		// See if we have a FILE line
		if (strstr(cueLine, "FILE") == cueLine) {
			char *start = strchr(cueLine, '"');
			char *end = strrchr(cueLine, '"');

			if (start && end && start != end) {
				// Valid file entry, isolate path
				binFound = true;
				++start;
				*end = '\0';

				// Copy binary path into linked list
				size_t tempPathSize = strlen(start);
				for (size_t i = 0; i < tempPathSize; ++i) {
					if (!LinkedList_addObject(binPath,
							(void *)(uintptr_t)start[i])) {
						fprintf(stderr, "PhilPSX: CD: Couldn't add character "
								"to linked list to form temporary path\n");
						goto cleanup_resources;
					}
				}
			}
			else {
				// Invalid file line, end search
				fprintf(stderr, "PhilPSX: CD: FILE line in cue file "
						"is malformed\n");
				goto cleanup_resources;
			}
		}
	}

	// Prepend binPath with full path if needed
	const char *lastSeparator = strrchr(cdPath, pathSeparator);
	if (lastSeparator) {
		// Prepend path to binPath here
		++lastSeparator;
		for (size_t i = 0; &cdPath[i] != lastSeparator; ++i) {
			if (!LinkedList_addObjectAt(binPath, i,
					(void *)(uintptr_t)cdPath[i])) {
				fprintf(stderr, "PhilPSX: CD: Couldn't prepend character "
						"to linked list to form full path\n");
				goto cleanup_resources;
			}
		}
	}

	// Convert linked list binPath to char array finalPath
	finalPath = malloc(sizeof(char) * (LinkedList_getSize(binPath) + 1));
	if (!finalPath) {
		fprintf(stderr, "PhilPSX: CD: Couldn't allocate finalPath array\n");
		goto cleanup_resources;
	}
	CD_convertListToString(binPath, finalPath);
	fprintf(stdout, "PhilPSX: CD: Bin file path is %s\n", finalPath);

	// Open bin file, get size, and map it
	size_t cdFileSize;
	if (!CD_openBin(finalPath, &cdFileDescriptor, &cdFileSize, &cdMapping)) {
		fprintf(stderr, "PhilPSX: CD: Failed to handle bin file\n");
		goto cleanup_resources;
	}

	// Determine CD layout (this helps us later on to read bytes from the
	// correct location) by continuing to parse the rest of the cue file
	int64_t gap = 2352 * 150; // Two-second gap initially
	bool cueFinished = false;
	while (!cueFinished) {
		LinkedList_wipeAllObjects(cueLineAsList);

		// Read line from cue file
		if (!CD_readLineToList(cueLineAsList, cueFileDescriptor)) {
			fprintf(stderr, "PhilPSX: CD: Failed to read a line to the "
					"linked list\n");
			goto cleanup_resources;
		}
		if (LinkedList_getSize(cueLineAsList) == 0) {
			// Reached end of file
			cueFinished = true;
			break;
		}

		// We have the line from the cue file, now lets trim it
		CD_trimLinkedList(cueLineAsList);

		// Convert line to C-style string to make use of standard
		// library functions
		char trackLine[LinkedList_getSize(cueLineAsList) + 1];
		CD_convertListToString(cueLineAsList, trackLine);

		// Check if this is a TRACK line
		if (strstr(trackLine, "TRACK") == trackLine) {
			// Declare type of track
			int32_t type;

			// Check for type of track from cueLine
			char *truncatedTrackLine = strrchr(trackLine, ' ');
			if (truncatedTrackLine) {
				// Check type
				++truncatedTrackLine;
				if (strstr(truncatedTrackLine, "AUDIO") == truncatedTrackLine)
					type = PHILPSX_TRACKTYPE_AUDIO;
				else if (strstr(truncatedTrackLine, "MODE2/2352") ==
						truncatedTrackLine)
					type = PHILPSX_TRACKTYPE_MODE2_2352;
				else {
					fprintf(stderr, "PhilPSX: CD: Unrecognised TRACK type\n");
					goto cleanup_resources;
				}

				LinkedList_wipeAllObjects(cueLineAsList);

				// Read the next line
				if (!CD_readLineToList(cueLineAsList, cueFileDescriptor)) {
					fprintf(stderr, "PhilPSX: CD: Failed to read a line to the "
							"linked list\n");
					goto cleanup_resources;
				}
				if (LinkedList_getSize(cueLineAsList) == 0) {
					// Should be a line here, error out
					fprintf(stderr, "PhilPSX: CD: No INDEX or PREGAP line\n");
					goto cleanup_resources;
				}

				// We have the line from the cue file, now lets trim it
				CD_trimLinkedList(cueLineAsList);

				// Convert line to C-style string to make use of standard
				// library functions
				char trackLine[LinkedList_getSize(cueLineAsList) + 1];
				CD_convertListToString(cueLineAsList, trackLine);

				if (strstr(trackLine, "PREGAP") == trackLine) {

					// Read PREGAP information
					int minutes, seconds, frames;
					if (!CD_getTimesFromLine(trackLine, &minutes,
							&seconds, &frames)) {
						fprintf(stderr, "PhilPSX: CD: Failed to read details "
								"from PREGAP line\n");
						goto cleanup_resources;
					}

					// Define tempGap
					int64_t tempGap = frames * 2352 + seconds *176400 + minutes
							* 10584000;
					gap += tempGap;

					LinkedList_wipeAllObjects(cueLineAsList);

					// Read the next line
					if (!CD_readLineToList(cueLineAsList, cueFileDescriptor)) {
						fprintf(stderr, "PhilPSX: CD: Failed to read a line to "
								"the linked list\n");
						goto cleanup_resources;
					}
					if (LinkedList_getSize(cueLineAsList) == 0) {
						fprintf(stderr, "PhilPSX: CD: No INDEX line after "
								"PREGAP line\n");
						goto cleanup_resources;
					}

					// We have the line from the cue file, now lets trim it
					CD_trimLinkedList(cueLineAsList);

					// Convert line to C-style string to make use of standard
					// library functions
					char trackLine[LinkedList_getSize(cueLineAsList) + 1];
					CD_convertListToString(cueLineAsList, trackLine);

					// Read track start information
					if (!CD_getTimesFromLine(trackLine, &minutes,
							&seconds, &frames)) {
						fprintf(stderr, "PhilPSX: CD: Failed to read details "
								"from INDEX line\n");
						goto cleanup_resources;
					}

					// Calculate track start
					int64_t trackStart = frames * 2352 + seconds * 176400 +
							minutes * 10584000 + gap;

					// Add track
					if (!CD_addTrackToList(trackList, type, trackStart - tempGap
							- 1, trackStart, gap)) {
						fprintf(stderr, "PhilPSX: CD: Failed to add a track to "
								"the list\n");
						goto cleanup_resources;
					}
				}
				else {

					// Read track start information
					int minutes, seconds, frames;
					if (!CD_getTimesFromLine(trackLine, &minutes,
							&seconds, &frames)) {
						fprintf(stderr, "PhilPSX: CD: Failed to read details "
								"from INDEX line\n");
						goto cleanup_resources;
					}

					// Calculate track start
					int64_t trackStart = frames * 2352 + seconds * 176400 +
							minutes * 10584000 + gap;

					// Add track
					if (!CD_addTrackToList(trackList, type, trackStart - 1,
							trackStart, gap)) {
						fprintf(stderr, "PhilPSX: CD: Failed to add a track to "
								"the list\n");
						goto cleanup_resources;
					}
				}
			}
			else {
				fprintf(stderr, "PhilPSX: CD: Malformed TRACK line\n");
				goto cleanup_resources;
			}
		}
	}

	// Calculate end of last track
	if (ArrayList_getSize(trackList) > 0) {
		CDTrack *cdt =
			ArrayList_getObject(trackList, ArrayList_getSize(trackList) - 1);
		cdt->trackEnd = cdFileSize + cdt->offset - 1;
	}
	else {
		fprintf(stderr, "PhilPSX: CD: No tracks found\n");
		goto cleanup_resources;
	}

	// Swap values
	if (!CD_swapState(cd, cdFileDescriptor, cdFileSize, cdMapping, trackList)) {
		fprintf(stderr, "PhilPSX: CD: Failed to swap state over to new file\n");
		goto cleanup_resources;
	}
	
	// Normal return:
	
	// Close cue file
	if (close(cueFileDescriptor) != 0) {
		fprintf(stderr, "PhilPSX: CD: Couldn't close cue file during "
				"error cleanup\n");
	}
	
	// Ckeanup linked lists
	destruct_LinkedList(binPath);
	destruct_LinkedList(cueLineAsList);
	
	// Cleanup finalPath
	free(finalPath);
	
	return retVal;
	
	// Cleanup path:
	cleanup_resources:
	if (cdMapping) {
		// Remove mapping
		if (munmap(cdMapping, cdFileSize) != 0) {
			fprintf(stderr, "PhilPSX: CD: Couldn't unmap file mapping "
					"from address space during error cleanup\n");
		}
	}

	if (cdFileDescriptor != -1) {
		// Close file
		if (close(cdFileDescriptor) != 0) {
			fprintf(stderr, "PhilPSX: CD: Couldn't close CD file during "
					"error cleanup\n");
		}
	}
	
	if (trackList)
		destruct_ArrayList(trackList);
	if (binPath)
		destruct_LinkedList(binPath);
	if (cueLineAsList)
		destruct_LinkedList(cueLineAsList);
	if (finalPath)
		free(finalPath);
	
	if (cueFileDescriptor != -1) {
		// Close file
		if (close(cueFileDescriptor) != 0) {
			fprintf(stderr, "PhilPSX: CD: Couldn't close cue file during "
					"error cleanup\n");
		}
	}

	retVal = false;
	return retVal;
}

/*
 * This function reads a byte from the CD image file.
 */
int8_t CD_readByte(CD *cd, int64_t position)
{
	// Declare return value
	int8_t retVal = 0;

	for (int i = 0; i < ArrayList_getSize(cd->trackList); ++i) {
		CDTrack *cdt = ArrayList_getObject(cd->trackList, i);
		if (CDTrack_isWithin(cdt, position)) {
			position -= cdt->offset;
			retVal = cd->cdMapping[(int32_t)position];
			break;
		}
	}

	// Return byte
	return retVal;
}

/*
 * This function operates under the assumption that the void * pointer inside
 * each node of the list is actually storing a type-casted char value rather
 * than a valid pointer. It trims spaces and tabs from the ends of the list
 * based on this assumption.
 */
static void CD_trimLinkedList(LinkedList *list)
{
	// Declare variables to store temporary char and size of the list
	char c;
	size_t listSize;

	// Trim from beginning
	while ((listSize = LinkedList_getSize(list)) > 0) {
		c = (char)(uintptr_t)LinkedList_getObject(list, 0);
		if (c == ' ' || c == '\t')
			LinkedList_removeObject(list, 0);
		else
			break;
	}

	// Trim from end
	while ((listSize = LinkedList_getSize(list)) > 0) {
		c = (char)(uintptr_t)LinkedList_getObject(list, listSize - 1);
		if (c == ' ' || c == '\t')
			LinkedList_removeObject(list, listSize - 1);
		else
			break;
	}
}

/*
 * This function reads a line (as chars) from a file referenced by the supplied
 * file descriptor into the linked list referenced by list.
 */
static bool CD_readLineToList(LinkedList *list, int fileDescriptor)
{
	// Define return value with default value of success
	bool retVal = true;

	while (true) {
		char c;
		ssize_t readResult = read(fileDescriptor, &c, 1);
		if (readResult == -1) {
			// Failed to read file, set retVal to false
			fprintf(stderr, "PhilPSX: CD: Error reading file\n");
			retVal = false;
			break;
		}
		else if (readResult == 0) {
			// No more bytes, break
			break;
		}
		else if (c != '\n') {
			if (!LinkedList_addObject(list, (void *)(uintptr_t)c)) {
				// Failed to allocate memory for char in list so
				// set retVal to false
				fprintf(stderr, "PhilPSX: CD: Couldn't add character "
						"to linked list from file\n");
				retVal = false;
			}
			// Keep going, no break
		}
		else {
			// Character was a newline, break
			break;
		}
	}

	return retVal;
}

/*
 * This function reads (and modifies) the provides string (which should be
 * disposable) in order to store the values back to the relevant int locations.
 * It is intended to parse this information from the remainder of a PREGAP or
 * INDEX cue line.
 */
static bool CD_getTimesFromLine(char *line, int *minutes,
		int *seconds, int *frames)
{
	// Declare return value with default of true
	bool retVal = true;

	// Read timing information
	line = strrchr(line, ' ');
	if (!line) {
		fprintf(stderr, "PhilPSX: CD: Couldn't find space at end of PREGAP or "
				"INDEX line\n");
		goto end;
	}
	++line;
	char *tempTimePtr = strrchr(line, ':');
	if (!tempTimePtr) {
		fprintf(stderr, "PhilPSX: CD: Malformed duration in PREGAP or "
				"INDEX line\n");
		goto end;
	}
	++tempTimePtr;
	*frames = atoi(tempTimePtr);
	*(tempTimePtr - 1) = '\0';
	tempTimePtr = strrchr(line, ':');
	if (!tempTimePtr) {
		fprintf(stderr, "PhilPSX: CD: Malformed duration in PREGAP or "
				"INDEX line\n");
		goto end;
	}
	++tempTimePtr;
	*seconds = atoi(tempTimePtr);
	*(tempTimePtr - 1) = '\0';
	*minutes = atoi(line);

	// Normal return:
	return retVal;
	
	// Cleanup path:
	end:
	retVal = false;
	return retVal;
}

/*
 * This function swaps out and deallocates the old state of the CD object
 * before replacing it with the new state.
 */
static bool CD_swapState(CD *cd, int cdFileDescriptor, size_t cdFileSize,
		int8_t *cdMapping, ArrayList *trackList)
{
	// Define return value set to default of true
	bool retVal = true;

	// Close an existing image if it is loaded, and swap new values in
	if (cd->cdMapping) {

		// Remove mapping
		if (munmap(cd->cdMapping, cd->cdFileSize) != 0) {
			fprintf(stderr, "PhilPSX: CD: Couldn't unmap previous file mapping "
					"from address space\n");
			goto end;
		}

		// Close file
		if (close(cd->cdFileDescriptor) != 0) {
			fprintf(stderr, "PhilPSX: CD: Couldn't close previous CD file\n");
			goto end;
		}

		// Destruct track list
		destruct_ArrayList(cd->trackList);
	} else {
		// Just destruct track list
		destruct_ArrayList(cd->trackList);
	}
	
	// Swap values over
	cd->trackList = trackList;
	cd->cdFileDescriptor = cdFileDescriptor;
	cd->cdFileSize = cdFileSize;
	cd->cdMapping = cdMapping;

	// Normal return:
	return retVal;
	
	// Cleanup path:
	end:
	retVal = false;
	return retVal;
}

/*
 * This function takes a linked list representing a group of chars, and
 * deposits it into the destination buffer in C-style string format with
 * a null terminator.
 */
static void CD_convertListToString(LinkedList *list, char *dst)
{
	size_t listSize = LinkedList_getSize(list);
	for (size_t i = 0; i < listSize; ++i)
		dst[i] = (char)(uintptr_t)LinkedList_getObject(list, i);
	dst[listSize] = '\0';
}

/*
 * This function opens a bin file, gets its size, and maps it into the address
 * space using mmap. It returns the descriptor, size and mapping by
 * dereferencing the provided pointers.
 */
static bool CD_openBin(const char *path, int *cdFileDescriptor,
		size_t *cdFileSize, int8_t **cdMapping)
{
	// Define return value with default of true
	bool retVal = true;
	
	int tempFileDescriptor =  open(path, O_RDONLY);
	if (tempFileDescriptor == -1) {
		fprintf(stderr, "PhilPSX: CD: Couldn't open bin file\n");
		goto end;
	}
	ssize_t tempSignedFileSize = lseek(tempFileDescriptor, 0, SEEK_END);
	if (tempSignedFileSize == -1) {
		fprintf(stderr, "PhilPSX: CD: Couldn't seek to end of bin file\n");
		goto cleanup_openfile;
	}
	size_t tempFileSize = (size_t)tempSignedFileSize;
	if (lseek(tempFileDescriptor, 0, SEEK_SET) == -1) {
		fprintf(stderr, "PhilPSX: CD: Couldn't seek to beginning of "
				"bin file\n");
		goto cleanup_openfile;
	}
	int8_t *tempMapping =
		mmap(NULL, tempFileSize, PROT_READ, MAP_PRIVATE, tempFileDescriptor, 0);
	if (tempMapping == MAP_FAILED) {
		fprintf(stderr, "PhilPSX: CD: Couldn't map bin file into address "
				"space\n");
		goto cleanup_openfile;
	}
	
	// Normal return (store info back to caller values)
	*cdFileDescriptor = tempFileDescriptor;
	*cdFileSize = tempFileSize;
	*cdMapping = tempMapping;
	return retVal;
	
	// Cleanup path:
	cleanup_openfile:
	if (close(tempFileDescriptor)) {
		fprintf(stderr, "PhilPSX: CD: Couldn't close bin file during "
				"error path\n");
	}
	
	end:
	retVal = false;
	return retVal;
}

/*
 * This function is responsible for taking the parameters for a track and adding
 * it into the specified array list.
 */
static bool CD_addTrackToList(ArrayList *trackList, int32_t type,
		int64_t prevEnd, int64_t trackStart, int64_t gap)
{
	// Define return value with default of true
	bool retVal = true;
	
	// Set previous track end
	if (ArrayList_getSize(trackList) > 0) {
		CDTrack *cdt =
			ArrayList_getObject(trackList, ArrayList_getSize(trackList) - 1);
		cdt->trackEnd = prevEnd;
	}

	// Allocate track and add it to the list
	CDTrack *track =
		construct_CDTrack(ArrayList_getSize(trackList) + 1, type,
				trackStart, gap);
	if (!track) {
		fprintf(stderr, "PhilPSX: CD: Couldn't construct new track\n");
		goto end;
	}
	if (!ArrayList_addObject(trackList, track)) {
		fprintf(stderr, "PhilPSX: CD: Couldn't add track to list\n");
		goto cleanup_track;
	}
	
	// Normal return:
	return retVal;
	
	// Cleanup path:
	cleanup_track:
	destruct_CDTrack(track);
	
	end:
	retVal = false;
	return retVal;
}

/*
 * This function handles the presence of a UTF-8 byte order mark by advancing
 * the marker in the cue file past it if present.
 */
static bool CD_handleUtf8Bom(int cueFileDescriptor)
{
	// Define return value with default of true
	bool retVal = true;
	
	char bom[3];
	ssize_t readResult = read(cueFileDescriptor, bom, 3);
	if (readResult != 3) {
		fprintf(stderr, "PhilPSX: CD: Couldn't check cue file for UTF-8 BOM\n");
		goto end;
	}
	if (!(bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF)) {
		// BOM wasn't detected, seek back to start of file
		if (lseek(cueFileDescriptor, 0, SEEK_SET) == -1) {
			fprintf(stderr, "PhilPSX: CD: Couldn't seek back to beginning of "
					"cue file\n");
			goto end;
		}
	}
	
	// Normal return
	return retVal;
	
	// Cleanup path:
	end:
	retVal = false;
	return retVal;
}

/*
 * This function sets up a track for us.
 */
static CDTrack *construct_CDTrack(int32_t trackNumber, int32_t type,
		int64_t trackStart, int64_t offset)
{
	// Allocate track
	CDTrack *cdt = malloc(sizeof(CDTrack));
	if (!cdt) {
		fprintf(stderr, "PhilPSX: CD: CDTrack: Couldn't allocate memory for "
				"CDTrack struct\n");
		goto end;
	}
	
	// Set track properties
	cdt->trackNumber = trackNumber;
	cdt->type = type;
	cdt->trackStart = trackStart;
	cdt->trackEnd = -1L;
	cdt->offset = offset;
	
	end:
	return cdt;
}

/*
 * This function destructs a CDTrack object.
 */
static void destruct_CDTrack(CDTrack *cdt)
{
	free(cdt);
}

/*
 * This function allows us to use the real function as an automatic destructor
 * in a List type.
 */
static void destruct_CDTrack_Passthrough(void *cdt)
{
	destruct_CDTrack((CDTrack *)cdt);
}

/*
 * This function tells us whether the supplied byte reference is within
 * this track.
 */
static bool CDTrack_isWithin(CDTrack *cdt, int64_t position)
{
	return position >= cdt->trackStart && position <= cdt->trackEnd;
}
