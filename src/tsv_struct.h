/*-------------------------------------------------------------------*/
/*
We are having RDQ TSV and WDQ TSV
Each having Groups with 2 Sub Groups, Each Sub Group have 8 Byte Groups.
Each Byte Group have 8 Bits(bx) which represents default TSV and 
2 Bits(Rx) which represents the spare TSV for each byte group.
Each Sub group have a additional Bit(Rx) which represents the spare TSV for each sub group.
*/ 
/*-------------------------------------------------------------------*/

/* Macros Used of Shifting */
/*      Shifts        Values    Comments */
#define NO_SHIFT      (0)       // No shift
#define LEFT_SHIFT_1  (1)       // Shift 1 right
#define RIGHT_SHIFT_1 (2)       // Shift 1 left
#define RIGHT_SHIFT_2 (3)       // Shift 2 left

/*-------------------------------------------------------------------*/

/* Structure for a Byte Group */
typedef struct {
    unsigned int tsv : 10;  // 10 bit (8 - default TSV + 2 - spare TSV)
    unsigned char spare_used : 2; // Track if spares are used (bitmask: 0x1 for LSB, 0x2 for MSB)
} byte_group_t;

/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/

/* Structure for a Sub Group */
typedef struct 
{
    byte_group_t byte_group[8];  // 8 byte groups
    unsigned int rx : 1;         // 1 bit for spare TSV
} sub_group_t;

/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Structure for a TSV Group */
typedef struct 
{
    sub_group_t sub_group[2];  // 2 sub groups
} tsv_group_t;

/*-------------------------------------------------------------------*/
