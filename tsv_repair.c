// I want to simulate a TSV repair, where a hardware block is 64x6 pins that need to be tested.  Each setting is like the below:
//  o x x x x o 
// Where the x is the default location of a pin and the o is the spares for the block.  From each block of 6 bins, we must
//  must produce 4 pins and each pin can have the 4 following settings:
//   00 - The pin is at the location
//   01 - The pin is shifted 1 to the left
//   10 - The pin is shifted 1 to the right
//   11 - The pin is shifted 2 to the right
// It is possible for the pin to be shifted off the block (in the case of the 4th pin shifted 2 to the right).  In this case,
//  it is borrowed from the next block.

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
// We need the libary for memset
#include <string.h>
// Include bool as well
#include <stdbool.h>
// We will need to measure performance, and so we are going to use a library to let
//  us accurately run for 15 seconds
#include <time.h>
#include <sys/time.h>

// Define each of the DQ pins
struct dq_pin {
   unsigned char setting:2; // Contains the shift described above
};
#define DQ_SHIFT_0 (0)
#define DQ_SHIFT_1_LEFT (1)
#define DQ_SHIFT_1_RIGHT (2)
#define DQ_SHIFT_2_RIGHT (3)

struct dq_pin totaldqPins[256];

// Now we need to have the full block of pins, in the 64x6 configuration
struct tsv
{
   unsigned char tsvOperational:6;
   bool borrowedPin;
};

struct tsv tsvBlocks[64];

// So we need an algorithm to test the pins based on the outcome of the tsv_repair
//  this function takes the tsvPins and sets the DQ pins based on the settings.

// We should analyze this in blocks of 6 pins.
//  Let's create a lookup table for the settings, since there are only 64 possibilities
//  and thus we can set the dq based on that
struct tsvLookup
{
   struct dq_pin dqPins[4]; // Each index needs the setting for the 4 pins
   bool failed;
   bool mustBorrow;
   bool canLend;
};

struct tsvLookup tsvLookupTable[64] = {
   /* 000000 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, // Since all pins are not functional, this cannot function
   /* 000001 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, //  Not possible with 1 pin functional (we must have at least 3 pins functional)
   /* 000010 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, //  Not possible with 1 pin functional (we must have at least 3 pins functional)
   /* 000011 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, //  Not possible with 2 pin functional (we must have at least 3 pins functional)
   /* 000100 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, //  Not possible with 1 pin functional (we must have at least 3 pins functional)
   /* 000101 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, //  Not possible with 2 pin functional (we must have at least 3 pins functional)
   /* 000110 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, //  Not possible with 2 pin functional (we must have at least 3 pins functional)
   /* 000111 */ { { {DQ_SHIFT_2_RIGHT}, {DQ_SHIFT_2_RIGHT}, {DQ_SHIFT_2_RIGHT}, {DQ_SHIFT_2_RIGHT} }, false, true, false }, //  This is a borrow case, this is possible if we can shift all pins to the right by 2
   /* 001000 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, //  Not possible with 1 pin functional (we must have at least 3 pins functional)
   /* 001001 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, //  Not possible with 2 pin functional (we must have at least 3 pins functional)
   /* 001010 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, //  Not possible with 2 pin functional (we must have at least 3 pins functional)
   /* 001011 */ { { {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_2_RIGHT}, {DQ_SHIFT_2_RIGHT}, {DQ_SHIFT_2_RIGHT} }, false, true, false }, //  This is a borrow case, this is possible if we can shift all pins to the right by 2
   /* 001100 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, //  Not possible with 2 pin functional (we must have at least 3 pins functional)
   /* 001101 */ { { {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_2_RIGHT}, {DQ_SHIFT_2_RIGHT} }, false, true, false }, //  This is a borrow case, this is possible if we can shift all pins to the right by 2
   /* 001110 */ { { {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_2_RIGHT} }, false, true, false }, //  This is a borrow case, this is possible if we can shift all pins to the right by 2
   /* 001111 */ { { {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_1_RIGHT} }, false, false, false }, // With 4 pins functional, we should be able to find some shift that allows them all to work
   /* 010000 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, //  Not possible with 1 pin functional (we must have at least 3 pins functional)
   /* 010001 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, //  Not possible with 2 pin functional (we must have at least 3 pins functional)
   /* 010010 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, //  Not possible with 2 pin functional (we must have at least 3 pins functional)
   /* 010011 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_2_RIGHT}, {DQ_SHIFT_2_RIGHT}, {DQ_SHIFT_2_RIGHT} }, false, true, false }, //  This is a borrow case, this is possible if we can shift all pins to the right by 2
   /* 010100 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, //  Not possible with 2 pin functional (we must have at least 3 pins functional)
   /* 010101 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_2_RIGHT}, {DQ_SHIFT_2_RIGHT} }, false, true, false }, //  This is a borrow case, this is possible if we can shift all pins to the right by 2
   /* 010110 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_2_RIGHT} }, false, true, false }, //  This is a borrow case, this is possible if we can shift all pins to the right by 2
   /* 010111 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_1_RIGHT} }, false, false, false }, // With 4 pins functional, we should be able to find some shift that allows them all to work
   /* 011000 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, //  Not possible with 2 pin functional (we must have at least 3 pins functional)
   /* 011001 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_2_RIGHT}, {DQ_SHIFT_2_RIGHT} }, false, true, false }, //  This is a borrow case, this is possible if we can shift all pins to the right by 2
   /* 011010 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_2_RIGHT} }, false, true, false }, //  This is a borrow case, this is possible if we can shift all pins to the right by 2
   /* 011011 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_1_RIGHT} }, false, false, false }, // With 4 pins functional, we should be able to find some shift that allows them all to work
   /* 011100 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_2_RIGHT} }, false, true, false }, //  This is a borrow case, this is possible if we can shift all pins to the right by 2
   /* 011101 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_1_RIGHT} }, false, false, false }, // With 4 pins functional, we should be able to find some shift that allows them all to work
   /* 011110 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       false, false, false }, // This is the natural case, the 4 dq pins are all function, though the spares are not
   /* 011111 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       false, false, false }, // This is the natural case, the 4 dq pins are all function, though the spares are not
   /* 100000 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, //  Not possible with 1 pin functional (we must have at least 3 pins functional)
   /* 100001 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, //  Not possible with 2 pin functional (we must have at least 3 pins functional)
   /* 100010 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, //  Not possible with 2 pin functional (we must have at least 3 pins functional)
   /* 100011 */ { { {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_2_RIGHT}, {DQ_SHIFT_2_RIGHT}, {DQ_SHIFT_2_RIGHT} }, false, true, false }, //  This is a borrow case, this is possible if we can shift all pins to the right by 2
   /* 100100 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, //  Not possible with 2 pin functional (we must have at least 3 pins functional)
   /* 100101 */ { { {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_2_RIGHT}, {DQ_SHIFT_2_RIGHT} }, false, true, false }, //  This is a borrow case, this is possible if we can shift all pins to the right by 2
   /* 100110 */ { { {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_2_RIGHT} }, false, true, false }, //  This is a borrow case, this is possible if we can shift all pins to the right by 2
   /* 100111 */ { { {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_1_RIGHT} }, false, false, false }, // With 4 pins functional, we should be able to find some shift that allows them all to work
   /* 101000 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, //  Not possible with 2 pin functional (we must have at least 3 pins functional)
   /* 101001 */ { { {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_0},       {DQ_SHIFT_2_RIGHT}, {DQ_SHIFT_2_RIGHT} }, false, true, false }, //  This is a borrow case, this is possible if we can shift all pins to the right by 2
   /* 101010 */ { { {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_0},       {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_2_RIGHT} }, false, true, false }, //  This is a borrow case, this is possible if we can shift all pins to the right by 2
   /* 101011 */ { { {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_0},       {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_1_RIGHT} }, false, false, false }, // With 4 pins functional, we should be able to find some shift that allows them all to work
   /* 101100 */ { { {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_2_RIGHT} }, false, true, false }, //  This is a borrow case, this is possible if we can shift all pins to the right by 2
   /* 101101 */ { { {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_1_RIGHT} }, false, false, false }, // With 4 pins functional, we should be able to find some shift that allows them all to work
   /* 101110 */ { { {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       false, false, false }, // This is the natural case, the 4 dq pins are all function, though the spares are not
   /* 101111 */ { { {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_1_RIGHT} }, false, false, true },  // This is the first case where we can have 2 possibilities (a canLend case)
   /* 110000 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       true, false, false }, //  Not possible with 2 pin functional (we must have at least 3 pins functional)
   /* 110001 */ { { {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_2_RIGHT}, {DQ_SHIFT_2_RIGHT} }, false, true, false }, //  This is a borrow case, this is possible if we can shift all pins to the right by 2
   /* 110010 */ { { {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_2_RIGHT} }, false, true, false }, //  This is a borrow case, this is possible if we can shift all pins to the right by 2
   /* 110011 */ { { {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_1_RIGHT} }, false, false, false }, // With 4 pins functional, we should be able to find some shift that allows them all to work
   /* 110100 */ { { {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_0},       {DQ_SHIFT_2_RIGHT} }, false, true, false }, //  This is a borrow case, this is possible if we can shift all pins to the right by 2
   /* 110101 */ { { {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_0},       {DQ_SHIFT_1_RIGHT} }, false, false, false }, // With 4 pins functional, we should be able to find some shift that allows them all to work
   /* 110110 */ { { {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_0},       {DQ_SHIFT_0} },       false, false, false }, // This is the natural case, the 4 dq pins are all function, though the spares are not
   /* 110111 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_1_RIGHT} }, false, false, true },  // This is the first case where we can have 2 possibilities (a canLend case)
   /* 111000 */ { { {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_2_RIGHT} }, false, true, false }, // This is the natural case, the 4 dq pins are all function, though the spares are not
   /* 111001 */ { { {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_1_RIGHT} }, false, false, false }, // With 4 pins functional, we should be able to find some shift that allows them all to work
   /* 111010 */ { { {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_0} },       false, false, false }, // This is the natural case, the 4 dq pins are all function, though the spares are not
   /* 111011 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_1_RIGHT}, {DQ_SHIFT_1_RIGHT} }, false, false, true },  // This is the first case where we can have 2 possibilities (a canLend case)
   /* 111100 */ { { {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_1_LEFT},  {DQ_SHIFT_1_LEFT} },  false, false, false }, // This is the natural case, the 4 dq pins are all function, though the spares are not   
   /* 111101 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_1_RIGHT} }, false, false, true },  // This is the first case where we can have 2 possibilities (a canLend case)
   /* 111110 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       false, false, true }, // This is the natural case, the 4 dq pins are all function, though the spares are not
   /* 111111 */ { { {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0},       {DQ_SHIFT_0} },       false, false, true } // This is the natural case, the 4 dq pins are all function, though the spares are not
};

bool sanityTestTable(struct tsv tsvBlocks[64], struct dq_pin totaldqPins[256])
{
   // The first DQ pin is actually the second 0-based pin because it skips the spare
   unsigned int pinOffset = 1;

   // We need to create a mapping of all of the DQ pins to ensure that the setings never overlap (i.e. that the same pin is not used twice)
   int totalPins[64 * 6];
   memset(totalPins, -1, sizeof(totalPins));
   for (int i = 0; i < 256; i++)
   {
      int dq_shift = 0;
      if (totaldqPins[i].setting == DQ_SHIFT_0)
      {
         dq_shift = 0;
      }
      else if (totaldqPins[i].setting == DQ_SHIFT_1_LEFT)
      {
         dq_shift = -1;
      }
      else if (totaldqPins[i].setting == DQ_SHIFT_1_RIGHT)
      {
         dq_shift = 1;
      }
      else if (totaldqPins[i].setting == DQ_SHIFT_2_RIGHT)
      {
         dq_shift = 2;
      }

      // First we should check if the TSV is already assigned:
      if (totalPins[i + pinOffset + dq_shift] != -1)
      {
         printf("DQ Pin %d (setting=%u) is set to TSV which already has pin (%d) (TSV=%u)\n", 
                  i, totaldqPins[i].setting, totalPins[i + pinOffset + dq_shift], i + pinOffset + dq_shift);
         return false;
      }
      else
      {
         totalPins[i + pinOffset + dq_shift] = i;
      }

      // Now we should check if the TSV is good
      if ((tsvBlocks[(i + pinOffset + dq_shift) / 6].tsvOperational & (1 << (5 - (i + pinOffset + dq_shift) % 6))) == 0)
      {
         printf("DQ Pin %d (setting=%u) is set to a bad TSV (TSV=%u)\n", i, totaldqPins[i].setting, i + pinOffset + dq_shift);
         // Print out the TSV block and the binary mapping of the TSV block
         printf("TSV Block %d (%02x)\n", (i + pinOffset + dq_shift) / 6, tsvBlocks[(i + pinOffset + dq_shift) / 6].tsvOperational);
         return false;
      }

      // Once we have done 4 pins, we need to increment the offset by 2
      if ((i + 1) % 4 == 0)
      {
         pinOffset += 2;
      }
   }
   return true;
}

// This function will set the DQ 256 DQ pins based on the tsv pin values using the lookup table in blocks of 6
//  We will need to carry the borrow bit because in the situation where a bit is borrowed, we will need to change
//  the next TSV block lookup to see if the bit can be borrowed from it's neighbor if the canLend value is false
//  If the canLend value in the lookup is true, then the DQ mapping does not need to change
bool InitializeDQ(struct tsv tsvBlocks[64], struct dq_pin totaldqPins[256], bool printOutput)
{
   bool mustBorrow = false;
   bool chainBorrowed = false;
   struct tsvLookup lookup, borrowLookup; 
   
   for (int i = 0; i < 64; i++)
   {
      lookup = tsvLookupTable[tsvBlocks[i].tsvOperational];
      
      if (lookup.failed)
      {
         // In the failure case, print out the TSV block that we fail on
         //  and the binary mapping of the TSV block
         if (printOutput)
         {
            // Print out the adjacent TSVs as well
            printf("\nTSV Block %d failed - Intrinsically bad (%02x)\n", i, tsvBlocks[i].tsvOperational);
            if (i < 63) printf("TSV Block %d (%02x)\n", i + 1, tsvBlocks[i + 1].tsvOperational);
         }
         return false;
      }

      if (mustBorrow && !lookup.canLend)
      {
         // We must first see if the pin is available to be borrowed in the first place
         if ((tsvBlocks[i].tsvOperational & 0x20) == 0)
         {
            if (printOutput)
            {
               // Print out the adjacent TSVs as well
               printf("\nTSV Block %d failed - Borrow Failed (%02x)\n", i, tsvBlocks[i].tsvOperational);
               if (i < 63) printf("TSV Block %d (%02x)\n", i + 1, tsvBlocks[i + 1].tsvOperational);
            }
            return false;
         }

         // We can check if the lookup that strips off the top TSV also works
         borrowLookup = tsvLookupTable[tsvBlocks[i].tsvOperational & 0x1F];
         if (borrowLookup.failed)
         {
            if (printOutput)
            {
               // Print out the adjacent TSVs as well
               printf("\nTSV Block %d failed - Chain (%02x)\n", i, tsvBlocks[i].tsvOperational);
               if (i < 63) printf("TSV Block %d (%02x)\n", i + 1, tsvBlocks[i + 1].tsvOperational);
            }
            return false;
         }
         // We can use this as an alternative, which will likely carry the borrow bit in a subsequent lookup
         lookup = tsvLookupTable[tsvBlocks[i].tsvOperational & 0x1F];
      }
      
      if (lookup.mustBorrow)
      {
         mustBorrow = true;
      }
      else
      {
         mustBorrow = false;
      }

      if (printOutput)
      {
         if (i % 4 == 0)
         {
            printf("DQ Pin %d:\t", i*4);
         }
         // Print the TSV pins here
         printf("(%02x-%x)", tsvBlocks[i].tsvOperational, mustBorrow);
      }
      for (int j = 0; j < 4; j++)
      {
         totaldqPins[i*4+j] = lookup.dqPins[j];
         if (printOutput)
         {
            printf("%d", totaldqPins[i*4+j].setting);
         }
      }
      if (printOutput)
      {
         if (i % 4 == 3)
         {
            printf("\n");
         }
         else
         {
            printf("\t");
         }
      }
   }

   // If mustBorrow is still true, then we are dead
   if (mustBorrow)
   {
      if (printOutput)
      {
         printf("TSV Block %d failed - Chain Borrowed at final block (%02x)\n", 63, tsvBlocks[63].tsvOperational);
      }
      return false;
   }

   if (!sanityTestTable(tsvBlocks, totaldqPins))
   {
      return false;
   }

   return !mustBorrow;
}

enum TSV_TEST_STAGE
{
   TSV_TEST_DONE,
   TSV_TEST_VOLTAGE,
   TSV_TEST_CURRENT,
   TSV_TEST_PATTERN,
};

enum TSV_TEST_RESULT
{
   TSV_UNITIALIZED,
   TSV_TEST_PASS,
   TSV_TEST_FAIL_VOLTAGE,
   TSV_TEST_FAIL_CURRENT,
   TSV_TEST_FAIL_PATTERN,
};

struct tsvTestReg
{
   volatile int testDone:1;
   volatile int testResult:1;
   volatile int reserved:28;
   volatile int testStartRegister:2;
};

// This is the approximate style that I would imagine that we would have for the TSV test hardware.
struct tsvTestReg tsvTestReg[64 * 6] = {0};

// Let's have a register push function to operate on the TSV pins since we don't do HW simulation yet
void pushRegister(struct tsvTestReg *reg, enum TSV_TEST_STAGE stage, enum TSV_TEST_RESULT result)
{
   reg->testStartRegister = stage;

   // Here the HW reacts by immediately clearing the testDone and testResult registers
   reg->testDone = 0;
   reg->testResult = 0;
   
   // Now since we don't have a HW thread to simulate the results, we'll just immediately set the results here
   if (reg->testStartRegister != 0)
   {
      // This is the HW "starting" the pin test
      reg->testDone = 1;
      // @todo, come up with some criteria for the test
      reg->testResult = 1;

      // If we wanted to do the simulation, what we would need to do here is have a thread that checks tests that are
      //  running 
   }
}

#if 0
void InitializeTSV_Simulator(struct tsv tsvBlocks[64])
{
   struct tsvTester
   {
      unsigned int TSVsToTest;
      unsigned int goodPins;
      unsigned int badPins;
      unsigned int pinOffset; // Keeps track of what pins we are running the test on in the offset of 6
      unsigned int pinBlock; // A helper that keeps track of the block of pins we are testing out of 64
      enum TSV_TEST_STAGE stage; // This is the current stage of the test that we would run
      unsigned int pinsCurrentlyTesting; // This is the current count of pin that we are testing
   } tsvTestHarness = {0};
   tsvTestHarness.TSVsToTest = 64 * 6;
   tsvTestHarness.stage = TSV_TEST_VOLTAGE; // We start with the Voltage test

   // This is the data structure that each pin will use to run it's tests
   struct tsvInitializer
   {
      struct tsvTester *parent;
      unsigned int stage;
      unsigned int result;
      volatile int *testRegister;
   } tsvTester[64 * 6] = {0};

   // For each of the TSV pins, we need to create a test register that will allow us to test the pins
   for (int i = 0; i < 64; i++)
   {
      for (int j = 0; j < 6; j++)
      {
         tsvTester[i*6+j].parent = &tsvTestHarness;
         tsvTester[i*6+j].testRegister = &tsvTestReg[i*6+j].testDone;
      }
   }

   // Now imagine that we are in the HSC CPU environment, and we need to go through a process that validates the voltage connection, the current connection, and a pattern test
   //  for each of the DQ pins.  In the original FW design, we would be able to acuate a register that would allow us to test each of the TSV lanes, with some rules and 
   //  then likely we would need to check a result register.  We could simulate that here and then therefore match the behavior that is in the HSC CPU environment as a single 
   //  thread.

   bool runTSVTest = true;
   // So like here is the fascimile of the HSC CPU environment, operating a thread
   do
   {
      // So in the original design, there are 2 loops, one that is starting tests, and the other that is finishing test.
      //  Since there will be some consideration for adjacency, let's do the tests in loops of 64, 1 pin at a time so that
      //  it matches the maximum offset
      if (runTSVTest)
      {
         // For simple operation, we should only run if there are no outstanding tests
         if (tsvTestHarness.pinsCurrentlyTesting == 0)
         {
            // First we should check if we are done with the current stage (i.e. all pins have been tested indicated
            //  by the pinOffset being 6)
            if (tsvTestHarness.pinOffset == 6)
            {
               // We need to check if we are done with the current stage
               if (tsvTestHarness.stage == TSV_TEST_PATTERN)
               {
                  // We are done with the current stage
                  tsvTestHarness.stage = TSV_TEST_DONE;
               }
               else
               {
                  // We need to move to the next stage
                  tsvTestHarness.stage = (enum TSV_TEST_STAGE)((int)tsvTestHarness.stage + 1);
                  tsvTestHarness.pinOffset = 0;
               }
            }

            // Here we should run the tests of the pins assuming that we haven't finished submitting all of the pin tests
            if (tsvTestHarness.stage != TSV_TEST_DONE)
            {
               // Okay, start from the first block and run a single pin at pin offset
               for (int i = 0; i < 64; i++)
               {
                  // We need to run the test on the pin
                  pushRegister(tsvTester[i*6+j].testRegister, tsvTestHarness.stage, TSV_UNITIALIZED);
                  tsvTestHarness.pinsCurrentlyTesting++;
                  tsvTestHarness.pinBlock = i;
                  tsvTester[i*6+j].stage = tsvTestHarness.stage;
                  tsvTester[i*6+j].result = TSV_UNITIALIZED;
               }
               tsvTestHarness.pinOffset++;
            }
         }
      }

      if (tsvTestHarness.pinsCurrentlyTesting > 0)
      {
         // We need to check if the tests are done
         for (int i = 0; i < 64; i++)
         {
            for (int j = 0; j < 6; j++)
            {
               if (tsvTester[i*6+j].result == TSV_UNITIALIZED && tsvTester[i*6+j].stage != TSV_TEST_DONE)
               {
                  // We need to check if the test is done
                  if (tsvTester->testRegister->testDone)
                  {
                     // We need to check the result
                     tsvTester[i*6+j].result = tsvTestReg[i*6+j].testResult;
                     tsvTestHarness.pinsCurrentlyTesting--;

                     // If the test failed, we need to update the harness
                     if (tsvTester[i*6+j].result != TSV_TEST_PASS)
                     {
                        tsvTester->parent->badPins++;
                     }
                     // If the stage is at the pattern test and it passes, then this is a good pin
                     else if (tsvTester[i*6+j].stage == TSV_TEST_PATTERN)
                     {
                        tsvTester->parent->goodPins++;
                        // Set this in the structure in the TSV block so that we can use it in the DQ initialization
                        tsvBlocks[tsvTestHarness.pinBlock].tsvOperational |= (1 << j);
                     }
                  }
               }
            }
         }
      }
   } while (tsvTestHarness.stage != TSV_TEST_DONE);
}
#endif

// We need to create a function that helps us initialize the TSV pin blocks, and we want to do this by
//  randomly assigning each pin a specified probabiliy to be functional
void InitializeTSV(struct tsv tsvBlocks[64], float probability)
{
   memset(tsvBlocks, 0, sizeof(struct tsv) * 64);
   for (int i = 0; i < 64; i++)
   {
      for (int j = 0; j < 6; j++)
      {
         if ((float)rand()/(float)RAND_MAX < probability)
         {
            tsvBlocks[i].tsvOperational |= (1 << j);
         }
      }
   }
}

void dumpTSV(struct tsv tsvBlocks[64])
{
   for (int i = 0; i < 64; i++)
   {
      printf("TSV Block %d: %02xb\n", i, tsvBlocks[i].tsvOperational);   
   }
}

// This is our test harness, it will create a random TSV block, and then attempt to initialize the DQ pins
//  based on the TSV block.  If the DQ pins are initialized, then we will print out the DQ pins
int main (int argc, void *argv)
{
   bool testFailed = false;
   float probability = 1;
   // Seed the random number generator
   srand(time(NULL));
   
   do
   {
      probability -= 0.01;
      // Generate a TSV mapping, and then test to see if it works
      InitializeTSV(tsvBlocks, probability);
      testFailed = !InitializeDQ(tsvBlocks, totaldqPins, false);
      printf("Test Results: %d with probability %f\n", !testFailed, probability);
   } while (!testFailed);
   
   printf("TSV Repair Probability at Failure: %f\n", probability);
   // dumpTSV(tsvBlocks);  
   // Run the failing test again with the prints
   testFailed = !InitializeDQ(tsvBlocks, totaldqPins, true);

   return 0;
}