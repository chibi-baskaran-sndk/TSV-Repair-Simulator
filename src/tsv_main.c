#include "tsv_struct.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

tsv_group_t rdq_tsv; // Instance of RDQ TSV
tsv_group_t wdq_tsv; // Instance of WDQ TSV

/* This function initializes the TSV by setting it as working or not with respect to the probability.
   The LSB and MSB of the tsv field are always set to 1 (spare TSVs). */
void initializeTSV(tsv_group_t *tsvGroup, float probability)
{
    memset(tsvGroup, 0, sizeof(tsv_group_t)); // Initialize the TSV structure to zero

    for (int i = 0; i < 2; i++) // For each sub group
    {
        for (int j = 0; j < 8; j++) // For each byte group
        {
            // Set the LSB and MSB of tsv to 1 (spare TSVs)
            tsvGroup->sub_group[i].byte_group[j].tsv |= (1 << 0); // Set LSB (bit 0) because it is spare TSV
            tsvGroup->sub_group[i].byte_group[j].tsv |= (1 << 9); // Set MSB (bit 9) because it is spare TSV

            // Randomly set the middle 8 bits (default TSVs) based on probability
            for (int k = 1; k <= 8; k++) // Bits 1 to 8
            {
                float random_value = (float)rand() / (float)RAND_MAX;
                if (random_value < probability)
                {
                    tsvGroup->sub_group[i].byte_group[j].tsv |= (1 << k); // Set the bit to 1 (working TSV)
                }
            }
        }
        tsvGroup->sub_group[i].rx = 1; // Set the sub group spare TSV to working
    }
}

/* This function prints the initialized TSV structure in binary format */
void dumpTSV(tsv_group_t *tsvGroup)
{
    for (int i = 0; i < 2; i++)
    {
        printf("Sub Group %d:\n", i);
        for (int j = 0; j < 8; j++)
        {
            printf("  Byte Group %d: ", j);
            for (int k = 9; k >= 0; k--) // Print each bit of the 10-bit tsv field
            {
                printf(" %d ", (tsvGroup->sub_group[i].byte_group[j].tsv >> k) & 1);
            }
            printf("\n");
        }
        printf("  Spare TSV: %d\n", tsvGroup->sub_group[i].rx);
    }
}

// This function checks the TSV group for any issues. If there are more than 2 pins set to 0 in a byte group, then it is considered an issue.
// print which byte group has issues and the number of pins set to 0.
void checkTSV(tsv_group_t *tsvGroup)
{
    for (int i = 0; i < 2; i++) // For each sub group
    {
        for (int j = 0; j < 8; j++) // For each byte group
        {
            int failed_tsv_count = 0;
            for (int k = 1; k <= 8; k++) // Check bits 1 to 8
            {
                if ((tsvGroup->sub_group[i].byte_group[j].tsv & (1 << k)) == 0)
                {
                    // shiftTSV(&tsvGroup->sub_group[i].byte_group[j]);
                    failed_tsv_count++;
                }
            }
            if (failed_tsv_count > 0) // any pins set to 0 is an issue
            {
                printf("Issue in Sub Group %d, Byte Group %d: %d pins set to 0\n", i, j, failed_tsv_count);
            }
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <probability>\n", argv[0]);
        return 1;
    }

    float probability = atof(argv[1]); // Convert command-line argument to float
    if (probability < 0.0 || probability > 1.0)
    {
        fprintf(stderr, "Error: Probability must be between 0.0 and 1.0\n");
        return 1;
    }

    srand((unsigned int)time(NULL));      // Seed the random number generator
    initializeTSV(&rdq_tsv, probability); // Initialize TSV with given probability
    dumpTSV(&rdq_tsv); // Print the TSV structure
    checkTSV(&rdq_tsv); // Check the RDQ TSV structure for any issues
    return 0;
}