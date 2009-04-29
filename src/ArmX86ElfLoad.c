#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "ArmX86Debug.h"
#include "ArmX86ElfLoad.h"

/* Looking for an ARM executable */
#define EI_NIDENT 	        16
#define ET_EXEC                 2
#define EM_ARM                  40

/* Looking for loadable segments */
#define PT_LOAD                 1

struct elfHeader_t {
    unsigned        char e_ident[EI_NIDENT];/* Elf Identification */
    uint16_t        e_type;                 /* Relocatable/exe/so */
    uint16_t        e_machine;              /* Target Processor Family */
    uint32_t        e_version;              /* Object File version */
    uint32_t        e_entry;                /* Entry-Point Virt. Addr. */
    uint32_t        e_phoff;                /* Offset of program header */
    uint32_t        e_shoff;                /* Offset of section header */
    uint32_t        e_flags;                /* Processor Flags */
    uint16_t        e_ehsize;               /* Size of Elf header */
    uint16_t        e_phentsize;            /* Size of each prog. hdr. entry */
    uint16_t        e_phnum;                /* Num of prog. hdr. entries */
    uint16_t        e_shentsize;            /* Size of each sec. hdr entry */
    uint16_t        e_shnum;                /* Num of sec. hdr. entries */
    uint16_t        e_shstrndx;
};

struct programHeader_t {
    uint32_t        p_type;                 /* Classification of header */
    uint32_t        p_offset;               /* Offset of segment */
    uint32_t        p_vaddr;                /* Virt. Addr. for segment data */
    uint32_t        p_paddr;                /* Phys. Addr. for segment data */
    uint32_t        p_filesz;               /* Num. bytes of segment on file */
    uint32_t        p_memsz;                /* Num. bytes of segment in mem. */
    uint32_t        p_flags;
    uint32_t        p_align;
};

struct symTableEntry_t {
    uint32_t        st_name;
    uint32_t        st_value;
    uint32_t        st_size;
    unsigned char   st_info;
    unsigned char   st_other;
    uint16_t        st_shndx;
};

FILE *elf;

/*
 * Data structures to capture information about segments of the process image in
 * memory; used to map and unmap memory areas in the image.
 */
typedef enum segmentType_e {
    EXCLUSIVE,
    SUBSEGMENT,
    INTERSECTING,
    NUM_SEGMENT_TYPES
} segmentType_t;
char *segTypeString[NUM_SEGMENT_TYPES] = {"EXCLUSIVE", "SUBSEGMENT", "INTERSECTING"};

struct segment_t {
    struct programHeader_t *progHdr;
    struct segment_t *next;
    segmentType_t segType;
    int segmentMapped;
};

static struct segment_t *segmentList = NULL;

/*
 * FIXME: EOF handling (esp. premature) should be done better.
 * Convert these to inline functions.
 */
#define READ_ELF32_HALF(elem, elf)            \
    do {                                      \
        int numBytes;                         \
        numBytes = fread((elem), 1, 2, elf);  \
        if (numBytes < 2) {                   \
            DP("End of File reached\n");      \
        }                                     \
    } while(0)

#define READ_ELF32_WORD(elem, elf)            \
    do {                                      \
        int numBytes;                         \
        numBytes = fread((elem), 1, 4, elf);  \
        if (numBytes < 4) {                   \
            DP("End of File reached\n");      \
        }                                     \
    } while (0)

/*
 * Display the contents of an ELF header.
 *
 * Return: None
 */
static inline void
showElfHeader(const struct elfHeader_t *elfHeader)
{
    if (!elfHeader)
        return;

    debug(("Elf Header Identification: %s\n", elfHeader->e_ident));
    debug(("ELF Type: %d\n", elfHeader->e_type));
    debug(("ELF Machine: %d\n", elfHeader->e_machine));
    debug(("ELF Version: %d\n", elfHeader->e_version));
    debug(("ELF Entry: %p\n", (void *)elfHeader->e_entry));
    debug(("ELF Program Header Offset: 0x%x\n", elfHeader->e_phoff));
    debug(("ELF Section Header Offset: 0x%x\n", elfHeader->e_shoff));
    debug(("ELF Flags: 0x%x\n", elfHeader->e_flags));
    debug(("ELF Header Size: %d\n", elfHeader->e_ehsize));
    debug(("ELF Program Header Ent Size: %d\n", elfHeader->e_phentsize));
    debug(("ELF Program Header Num: %d\n", elfHeader->e_phnum));
    debug(("ELF Section Header Ent Size: %d\n", elfHeader->e_shentsize));
    debug(("ELF Section Header Num: %d\n", elfHeader->e_shnum));
    debug(("ELF Section Header Str Index: %d\n", elfHeader->e_shstrndx));
}

/*
 * Read the contents of the ELF header.
 *
 * Return: None
 */
static void
parseElfHeader(FILE *elf, struct elfHeader_t *elfHeader)
{
    int numBytesRead;

    DP_ASSERT(elfHeader != NULL, "Null ELF Header pointer");

    numBytesRead = fread(elfHeader->e_ident, 1, EI_NIDENT, elf);
    READ_ELF32_HALF(&elfHeader->e_type, elf);
    READ_ELF32_HALF(&elfHeader->e_machine, elf);
    READ_ELF32_WORD(&elfHeader->e_version, elf);
    READ_ELF32_WORD(&elfHeader->e_entry, elf);
    READ_ELF32_WORD(&elfHeader->e_phoff, elf);
    READ_ELF32_WORD(&elfHeader->e_shoff, elf);
    READ_ELF32_WORD(&elfHeader->e_flags, elf);
    READ_ELF32_HALF(&elfHeader->e_ehsize, elf);
    READ_ELF32_HALF(&elfHeader->e_phentsize, elf);
    READ_ELF32_HALF(&elfHeader->e_phnum, elf);
    READ_ELF32_HALF(&elfHeader->e_shentsize, elf);
    READ_ELF32_HALF(&elfHeader->e_shnum, elf);
    READ_ELF32_HALF(&elfHeader->e_shstrndx, elf);

    showElfHeader(elfHeader);
}

/*
 * Display the contents of an Program header.
 *
 * Return: None
 */
static inline void
showProgramHeader(const struct programHeader_t *programHeader)
{
    if (!programHeader)
        return;

    debug(("Program Header Type: %d\n", programHeader->p_type));
    debug(("Program Segment Offset: %d\n", programHeader->p_offset));
    debug(("Program Virtual Address: 0x%x\n", programHeader->p_vaddr));
    debug(("Program Physical Address: 0x%x\n", programHeader->p_paddr));
    debug(("Program Segment File Image Size: %d, 0x%x\n",
        programHeader->p_filesz,
        programHeader->p_filesz
    ));
    debug(("Program Segment Memory Image Size: %d, 0x%x\n",
        programHeader->p_memsz,
        programHeader->p_memsz
    ));
    debug(("Program Header Flags: 0x%x\n", programHeader->p_flags));
    debug(("Program Header Alignment: 0x%x\n", programHeader->p_align));
}

/*
 * Read the contents of the program header.
 *
 * The program header entries that are useful are the ones that
 * define loadable segments.
 *
 * There are two sizes defined for the data of a segment. One is
 * the size in the file, and the other is the size in memory. The
 * latter should always be larger. If the size in memory is larger,
 * it means that chunk should be set aside, and the empty space
 * following the bytes taken from the file should be populated
 * with zeroes.
 *
 * Return: None
 */
static void
parseProgramHeader(FILE *elf, struct programHeader_t *programHeader)
{
    DP_ASSERT(programHeader != NULL, "Null Program Header pointer");

    READ_ELF32_WORD(&programHeader->p_type, elf);
    READ_ELF32_WORD(&programHeader->p_offset, elf);
    READ_ELF32_WORD(&programHeader->p_vaddr, elf);
    READ_ELF32_WORD(&programHeader->p_paddr, elf);
    READ_ELF32_WORD(&programHeader->p_filesz, elf);
    READ_ELF32_WORD(&programHeader->p_memsz, elf);
    READ_ELF32_WORD(&programHeader->p_flags, elf);
    READ_ELF32_WORD(&programHeader->p_align, elf);

    showProgramHeader(programHeader);
}

/*
 * Check the header to see if the ELF is valid.
 *     Must be an ARM executable.
 *     FIXME: Check more conditions.
 *
 * Return: 0 is invalid
 *         1 otherwise
 */
static int
isElfValid(const struct elfHeader_t *elfHeader)
{
    if (elfHeader->e_type != ET_EXEC) {
        DP("Invalid ELF: Non-executable image\n");
        return 0;
    }

    if (elfHeader->e_machine != EM_ARM) {
        DP("Invalid ELF: Not ARM executable\n");
        return 0;
    }

    if (elfHeader->e_phoff == 0) {
        DP("Invalid ELF: Process image cannot be created\n");
        return 0;
    }

    return 1;
}

/*
 * Create a new segment_t instance for a segment in the process
 * image.
 *
 * Return: Address of the segment instance, or NULL
 */
static struct segment_t *
createSegment(struct programHeader_t *programHeader)
{
    struct segment_t * newSegment = malloc(sizeof(struct segment_t));
    if (!newSegment) {
	debug(("No memory for new segment descriptor\n"));
        return NULL;
    }

    newSegment->progHdr = programHeader;
    newSegment->next = NULL;
    newSegment->segType = EXCLUSIVE;
    newSegment->segmentMapped = 0;

    return newSegment;
}

/*
 * Add a  segment to a global list of segments.
 *
 * Return: None.
 */
static inline void
addSegmentToList(struct segment_t *seg)
{
    panic(seg != NULL, ("Null segment being added to list"));

    if (segmentList == NULL) {
        segmentList = seg;
	seg->next = NULL;
    } else {
        seg->next = segmentList;
	segmentList = seg;
    }
}

/* 
 * Clear the global list of segments.
 *
 * Return: None.
 */
static void
clearSegmentList()
{
    struct segment_t *temp = segmentList, *nexttemp;

    if (!segmentList) {
        debug(("No elements to clear in segment list\n"));
	return;
    }

    while (temp) {
        nexttemp = temp->next;
	free(temp);
	temp = nexttemp;
    }

    segmentList = NULL;
}

/*
 * Display all segments discovered thus far, and their properties.
 * To be used as a debuggin aid. Information is displayed only in
 * debug builds.
 *
 * Return: None
 */
static void
showSegments()
{
    struct segment_t *temp = segmentList;

    if (!segmentList) {
        debug(("No elements to show in segment list\n"));
	return;
    }

    while (temp) {
        debug(("Segment Start: 0x%08x, Size: %d, Type: %s\n",
	    temp->progHdr->p_vaddr, temp->progHdr->p_memsz,
	    segTypeString[temp->segType]));
	temp = temp->next;
    }
}

/*
 * Allocate space in the x86 process image for the ARM image segments.
 * There are expected to be text and data segments. Only exclusive segments
 * are mapped.
 *
 * Mapping a segment may fail for a variety of reasons.
 *
 * Return: -1 if mapping failed.
 *          0 if mapping ok.
 */
static int
mapSegments()
{
    struct segment_t *temp = segmentList;
    uintptr_t *addr;

    if (!segmentList) {
        debug(("No segments in list!\n"));
        return 0;
    }

    while (temp) {
        if (temp->segType == EXCLUSIVE) {
            debug(("Mapping segment starting at 0x%08x\n", temp->progHdr->p_vaddr));

            addr = mmap((void *)temp->progHdr->p_vaddr, temp->progHdr->p_memsz,
	                PROT_READ, MAP_ANONYMOUS | MAP_FIXED, -1, 0);

	    if (addr == (void *)-1) {
                sys_err(("Failed to map segment of executable"));
		return -1;
	    } else {
	        temp->segmentMapped = 1;
            }
        }

	temp = temp->next;
    }

    return 0;
}

/*
 * Unmap all the exclusive segments that have been mapped so far. A flag in
 * the segment data structure indicates whether the segment was mapped
 * successfully.
 *
 * Return: None.
 * FIXME: Check for the return valud of munmap.
 */
static void
unmapSegments()
{
    struct segment_t *temp = segmentList;

    if (!segmentList) {
	debug(("No segments in list!\n"));
        return;
    }
    
    while (temp) {
	debug(("Unmapping segment starting at 0x%08x\n", temp->progHdr->p_vaddr));

        if (temp->segType == EXCLUSIVE && temp->segmentMapped) {
            munmap((void *)temp->progHdr->p_vaddr, temp->progHdr->p_memsz);
	}

        temp = temp->next;
    }
}

/*
 * Initialize the mapped portion of the segment from the elf file. The size
 * of the segment in the file may be less than the size of the segment in
 * memory. The difference is required to be initialized to 0. This does not
 * have to be done explicitly. The anonymous mapping automatically initializes
 * the memory area to 0.
 *
 * Return: None
 */
static void
initSegments()
{
    struct segment_t *temp = segmentList;
    uint32_t numBytesRead;
    uint8_t *inst;

    if (!segmentList) {
        debug(("No segments to initialize in list"));
	return;
    }

    while (temp) {
        inst = (uint8_t *)(temp->progHdr->p_vaddr);
        fseek(elf, temp->progHdr->p_offset, SEEK_SET);
        numBytesRead = fread((void *)inst, 1, temp->progHdr->p_filesz, elf);
    
        if (temp->progHdr->p_filesz != 0) {
            panic(numBytesRead != 0, ("Encountered read error loading elf"));
        }

        temp = temp->next;
    }

}

/*
 * EXCLUSIVE
 *
 * | ref | ... | newSeg | or | newseg | ... | ref |
 *
 * INTERSECTING
 *
 * | ref |
 *    | newseg |
 *
 * | newseg |
 *       | ref |
 *
 * SUBSEGMENT
 *
 * |        ref        |
 *    |    newseg   |
 *
 * |  newseg  |
 *    | ref |
 *
 */
static int
compareSegments(struct segment_t *ref, struct segment_t *newseg)
{
    uint32_t refEnd, newsegEnd, refStart, newsegStart;

    panic(((ref != NULL) && (newseg != NULL)), ("Null segment addresses"));

    refStart = ref->progHdr->p_vaddr;
    refEnd = refStart + ref->progHdr->p_memsz;

    newsegStart = newseg->progHdr->p_vaddr;
    newsegEnd = newsegStart + newseg->progHdr->p_memsz;

    if ((newsegEnd > refEnd && newsegStart > refEnd) ||
	(newsegStart < refStart && newsegEnd < refStart)) {
	newseg->segType = EXCLUSIVE;
	debug(("Set type to EXCLUSIVE\n"));
	return 0;
    } else if (newsegStart >= refStart && newsegEnd <= refEnd) {
        newseg->segType = SUBSEGMENT;
	debug(("Set type to SUBSEGMENT\n"));
	return 1;
    } else if (newsegStart < refStart && newsegEnd > refEnd) {
	newseg->segType = EXCLUSIVE;
	ref->segType = SUBSEGMENT;
	return 1;
    } else {
        panic (0, ("Intersecting segments found in ELF. Unsupported!\n"));
    }
}

/*
 * Determine whether the segment is exclusive, subsegment or an intersecting
 * segment relative to all the segments discovered thus far.
 *
 * Return: None
 */
static void
classifySegment(struct segment_t *seg)
{
    struct segment_t *temp = segmentList;

    if (!segmentList) {
        debug(("No elements to compare with in segment list\n"));
	return;
    }

    while (temp) {
	if (compareSegments(temp, seg)) {
            break;
	}
	temp = temp->next;
    }
}

uint32_t *
armX86ElfLoad(char *elfFile)
{
    struct elfHeader_t elfHeader;
    struct programHeader_t *programHeader;
    uint32_t i;
    uint32_t *entryPoint = NULL;

    debug_in;

    if ((elf = fopen(elfFile, "rb")) == NULL) {
        sys_err(("Could not open %s", elfFile));
        return NULL;
    }

    parseElfHeader(elf, &elfHeader);
    debug(("Done parsing elf\n"));

    if (!isElfValid(&elfHeader)) {
	goto out_fclose;
    }
    debug(("Elf Header Valid\n"));

    /*
     * The Elf header points to an array of program headers. Each program
     * header points to a segment. Segments may overlap. What this means is
     * that one segment may either complete include another segment or may
     * begin before another one ends, and end beyond the other. The former
     * of these cases is the common case, and is handled. The latter is
     * flagged as an error for the moment - FIXME.
     */
    programHeader = malloc(sizeof(struct programHeader_t) * elfHeader.e_phnum);
    if (!programHeader) {
	debug(("No memory for program header table entries\n"));
        goto out_fclose;
    }

    for (i = 0; i < elfHeader.e_phnum; i++) {
	struct segment_t *newseg;
        fseek(elf, elfHeader.e_phoff + i * elfHeader.e_phentsize, SEEK_SET);
        parseProgramHeader(elf, &programHeader[i]);

        newseg = createSegment(&programHeader[i]);
	if (!newseg) {
            debug(("No memory for segment descriptor\n"));
	    goto out_freehdr;
	}

        classifySegment(newseg);
	addSegmentToList(newseg);
    }

    showSegments();

    /*
     * Go through the list of segments and for each exclusive segment, perform
     * an mmap of the size of the segment in memory starting at the virtual
     * address specified for the segment.
     */
    if (mapSegments() == -1) {
        goto out_unmap;
    }

    /*
     * Go through all segments, and initialize the mmap-ed space from the
     * segment information in the elf file.
     */
    initSegments();

    entryPoint = (uint32_t *)elfHeader.e_entry;
    goto out_done;

out_unmap:
    unmapSegments();

out_freehdr:
    clearSegmentList();
    free(programHeader);

out_fclose:
    fclose(elf);

out_done:
    debug_out;
    return entryPoint;;
}
