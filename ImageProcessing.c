#include <math.h>
#include <mpi.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    unsigned char r, g, b;
} Color;

#define FILTER_SMOOTH 0
#define FILTER_GAUSSIAN 1
#define FILTER_SHARP 2
#define FILTER_MEAN 3
#define FILTER_EMBOSS 4

int width;
int height;
int nOfFilters;

unsigned char clampToByte(float byte)
{
    unsigned char clampedByte;
    // byte += 0.5; // for rounding
    if (byte < 0)
        clampedByte = 0;
    else if (byte > 255)
        clampedByte = 255;
    else
        clampedByte = byte;
    return clampedByte;
}

unsigned char *packRedChannel(Color *vec, int length)
{
    unsigned char *red = malloc(length * sizeof(unsigned char));
    for (int i = 0; i < length; i++)
        red[i] = vec[i].r;

    return red;
}

unsigned char *packGreenChannel(Color *vec, int length)
{
    unsigned char *green = malloc(length * sizeof(unsigned char));
    for (int i = 0; i < length; i++)
        green[i] = vec[i].g;

    return green;
}

unsigned char *packBlueChannel(Color *vec, int length)
{
    unsigned char *blue = malloc(length * sizeof(unsigned char));
    for (int i = 0; i < length; i++)
        blue[i] = vec[i].b;

    return blue;
}

Color *unpackColor(unsigned char *red, unsigned char *green, unsigned char *blue, int length)
{
    Color *colors = malloc(length * sizeof(Color));
    for (int i = 0; i < length; i++)
    {
        colors[i].r = red[i];
        colors[i].g = green[i];
        colors[i].b = blue[i];
    }

    return colors;
}
unsigned char **readImagePGM(char *filename)
{
    FILE *fp;

    fp = fopen(filename, "rb");

    char type[10];
    int maxval;

    char *line = NULL;
    size_t len = 0;

    getline(&line, &len, fp);
    sscanf(line, "%s", type); // P5 or P6
    getline(&line, &len, fp); // Comment
    getline(&line, &len, fp); // width and height
    sscanf(line, "%d%d", &width, &height);
    getline(&line, &len, fp);
    sscanf(line, "%d", &maxval);

    unsigned char **img = (unsigned char **)calloc(height, sizeof(unsigned char *));

    for (int i = 0; i < height; i++)
        img[i] = (unsigned char *)calloc(width, sizeof(unsigned char));

    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
            fread(&img[i][j], 1, 1, fp);

    fclose(fp);
    return img;
}
void writeImagePGM(unsigned char **img, char *filename)
{
    FILE *out = fopen(filename, "wb");

    fprintf(out, "%s\n%d %d\n%d\n", "P5", width, height, 255);

    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
            fwrite(&img[i][j], 1, 1, out);

    fclose(out);
}

/* The offset tells us how deep to go in the extended rows of the image, to ensure that the
edge rows are calculated correctly when applying multiple filters consecutively on
multiple processes */
/* This changes the original image! */
unsigned char **applyFilterPGM(unsigned char **img, int filterID, int start, int end, int offset)
{
    int size = end - start;
    // linearized kernel matrices for each filter
    float rotK[9];
    {
        if (filterID == FILTER_SMOOTH)
        {
            float K[9] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
            for (int i = 0; i < 9; i++)
                rotK[i] = K[i] / 9.0;
        }
        else if (filterID == FILTER_SHARP)
        {
            float K[9] = {0, -2, 0, -2, 11, -2, 0, -2, 0};
            for (int i = 0; i < 9; i++)
                rotK[i] = K[i] / 3.0;
        }
        else if (filterID == FILTER_GAUSSIAN)
        {
            float K[9] = {1, 2, 1, 2, 4, 2, 1, 2, 1};
            for (int i = 0; i < 9; i++)
                rotK[i] = K[i] / 16.0;
        }
        else if (filterID == FILTER_MEAN)
        {
            float K[9] = {-1, -1, -1, -1, 9, -1, -1, -1, -1};
            for (int i = 0; i < 9; i++)
                rotK[i] = K[i] / 1.0;
        }
        else if (filterID == FILTER_EMBOSS)
        {
            float K[9] = {0, -1, 0, 0, 0, 0, 0, 1, 0};
            for (int i = 0; i < 9; i++)
                rotK[i] = K[i] / 1.0;
        }
        else
        {
            printf("Invalid Filter!\n");
            exit(0);
        }
    }
    /* the image is exetened 'nOfFilters' in each direction on height and bordered with 0 on width */
    unsigned char **borderedImg = (unsigned char **)calloc((size + 2 * nOfFilters), sizeof(unsigned char *));

    for (int i = 0; i < size + 2 * nOfFilters; i++)
        borderedImg[i] = (unsigned char *)calloc((width + 2), sizeof(unsigned char));

    for (int i = 0; i < size + 2 * nOfFilters; i++)
        for (int j = 0; j < width; j++)
            borderedImg[i][j + 1] = img[i][j];

    for (int i = 1 + offset; i < size + 2 * nOfFilters - offset - 1; i++)
        for (int j = 1; j < width + 1; j++)
        {
            float finalPixel = 0;
            for (int m = -1, c = 0; m <= 1; m++)
                for (int n = -1; n <= 1; n++, c++)
                    // original image's edges can't be extended, so we'll treat them as bordered with 0
                    if (!((start == 0 && i < nOfFilters) || (end == height && i > size + nOfFilters - 1)))
                        finalPixel += borderedImg[i + m][j + n] * rotK[c];

            img[i][j - 1] = clampToByte(finalPixel);
        }

    return img;
}
Color **readImagePNM(char *filename)
{
    FILE *fp;

    fp = fopen(filename, "rb");

    char type[10];
    int maxval;

    char *line = NULL;
    size_t len = 0;

    getline(&line, &len, fp);
    sscanf(line, "%s", type); // P5 or P6
    getline(&line, &len, fp); // Generated by Gimp
    getline(&line, &len, fp); // width and height
    sscanf(line, "%d%d", &width, &height);
    getline(&line, &len, fp);
    sscanf(line, "%d", &maxval);

    Color **img = (Color **)calloc(height, sizeof(Color *));

    for (int i = 0; i < height; i++)
        img[i] = (Color *)calloc(width, sizeof(Color));

    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
        {
            unsigned char r, g, b;
            fread(&r, 1, 1, fp);
            fread(&g, 1, 1, fp);
            fread(&b, 1, 1, fp);
            img[i][j].r = r;
            img[i][j].g = g;
            img[i][j].b = b;
        }

    fclose(fp);

    return img;
}
void writeImagePNM(Color **img, char *filename)
{
    FILE *out = fopen(filename, "wb");

    fprintf(out, "%s\n%d %d\n%d\n", "P6", width, height, 255);

    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
        {
            fwrite(&img[i][j].r, 1, 1, out);
            fwrite(&img[i][j].g, 1, 1, out);
            fwrite(&img[i][j].b, 1, 1, out);
        }
    fclose(out);
}

/* This changes the original image! */
Color **applyFilterPNM(Color **img, int filterID, int start, int end, int offset)
{
    int size = end - start;
    float rotK[9];
    {
        if (filterID == FILTER_SMOOTH)
        {
            float K[9] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
            for (int i = 0; i < 9; i++)
                rotK[i] = K[i] / 9.0;
        }
        else if (filterID == FILTER_SHARP)
        {
            float K[9] = {0, -2, 0, -2, 11, -2, 0, -2, 0};
            for (int i = 0; i < 9; i++)
                rotK[i] = K[i] / 3.0;
        }
        else if (filterID == FILTER_GAUSSIAN)
        {
            float K[9] = {1, 2, 1, 2, 4, 2, 1, 2, 1};
            for (int i = 0; i < 9; i++)
                rotK[i] = K[i] / 16.0;
        }
        else if (filterID == FILTER_MEAN)
        {
            float K[9] = {-1, -1, -1, -1, 9, -1, -1, -1, -1};
            for (int i = 0; i < 9; i++)
                rotK[i] = K[i] / 1.0;
        }
        else if (filterID == FILTER_EMBOSS)
        {
            float K[9] = {0, -1, 0, 0, 0, 0, 0, 1, 0};
            for (int i = 0; i < 9; i++)
                rotK[i] = K[i] / 1.0;
        }
        else
        {
            printf("Invalid Filter!\n");
            exit(0);
        }
    }

    Color **borderedImg = (Color **)calloc((size + 2 * nOfFilters), sizeof(Color *));

    for (int i = 0; i < size + 2 * nOfFilters; i++)
        borderedImg[i] = (Color *)calloc((width + 2), sizeof(Color));

    for (int i = 0; i < size + 2 * nOfFilters; i++)
        for (int j = 0; j < width; j++)
            borderedImg[i][j + 1] = img[i][j];

    for (int i = 1 + offset; i < size + 2 * nOfFilters - offset - 1; i++)
        for (int j = 1; j < width + 1; j++)
        {
            float finalPixelR = 0;
            float finalPixelG = 0;
            float finalPixelB = 0;
            for (int m = -1, c = 0; m <= 1; m++)
                for (int n = -1; n <= 1; n++, c++)
                    if (!((start == 0 && i < nOfFilters) || (end == height && i > size + nOfFilters - 1)))
                    {
                        finalPixelR += borderedImg[i + m][j + n].r * rotK[c];
                        finalPixelG += borderedImg[i + m][j + n].g * rotK[c];
                        finalPixelB += borderedImg[i + m][j + n].b * rotK[c];
                    }

            img[i][j - 1].r = clampToByte(finalPixelR);
            img[i][j - 1].g = clampToByte(finalPixelG);
            img[i][j - 1].b = clampToByte(finalPixelB);
        }

    return img;
}

int main(int argc, char *argv[])
{
    int rank;
    int nProcesses;
    MPI_Datatype colorType;
    MPI_Datatype type[3] = {MPI_UNSIGNED_CHAR, MPI_UNSIGNED_CHAR, MPI_UNSIGNED_CHAR};
    int blocklen[3] = {1, 1, 1};
    MPI_Init(&argc, &argv);

    MPI_Aint disp[3] = {offsetof(Color, r), offsetof(Color, g), offsetof(Color, b)};
    MPI_Type_create_struct(3, blocklen, disp, type, &colorType);
    MPI_Type_commit(&colorType);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nProcesses);

    int isColored;
    char *inName = argv[1];
    char *outName = argv[2];
    nOfFilters = argc - 3;
    int filters[nOfFilters];

    for (int i = 0; i < nOfFilters; i++)
    {
        if (strcmp(argv[i + 3], "smooth") == 0)
            filters[i] = FILTER_SMOOTH;
        else if (strcmp(argv[i + 3], "blur") == 0)
            filters[i] = FILTER_GAUSSIAN;
        else if (strcmp(argv[i + 3], "sharpen") == 0)
            filters[i] = FILTER_SHARP;
        else if (strcmp(argv[i + 3], "mean") == 0)
            filters[i] = FILTER_MEAN;
        else if (strcmp(argv[i + 3], "emboss") == 0)
            filters[i] = FILTER_EMBOSS;
        else
        {
            printf("[%s]Incorrect filter! Available: smooth, blur, sharpen, mean, emboss.\n", argv[i + 3]);
            exit(0);
        }
    }

    if (rank == 0)
    {
        if (strstr(inName, ".pnm") > 0)
        {
            isColored = 1;
            Color **img = readImagePNM(inName);
            int start, end;

            for (int i = 1; i < nProcesses; i++)
            {
                MPI_Send(&width, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                MPI_Send(&height, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                MPI_Send(&isColored, 1, MPI_INT, i, 0, MPI_COMM_WORLD);

                start = i * ceil((double)height / nProcesses);
                end = fmin(height, (i + 1) * ceil((double)height / nProcesses));

                // sending each process it's extended part of the image to process
                for (int j = start - nOfFilters; j < end + nOfFilters; j++)
                {
                    /* we need minimum 'nOfFilters' rows to extend to ensure the last filter has the
                     corect neighbours to perform it's calculations */
                    if (j < height)
                    {
                        MPI_Send(packRedChannel(img[j], width + 2), width + 2, MPI_UNSIGNED_CHAR, i, 0, MPI_COMM_WORLD);
                        MPI_Send(packGreenChannel(img[j], width + 2), width + 2, MPI_UNSIGNED_CHAR, i, 0,
                                 MPI_COMM_WORLD);
                        MPI_Send(packBlueChannel(img[j], width + 2), width + 2, MPI_UNSIGNED_CHAR, i, 0,
                                 MPI_COMM_WORLD);
                    }
                }
            }
            start = 0 * ceil((double)height / nProcesses);
            end = fmin(height, (0 + 1) * ceil((double)height / nProcesses) + nOfFilters);
            int size = end - start;

            // process 0 also must do it's job, so it unpacks his own part of the image (for method consistency)
            Color **img0 = (Color **)calloc(size + 2 * nOfFilters, sizeof(Color *));
            for (int j = 0; j < size + 2 * nOfFilters; j++)
                img0[j] = calloc(width, sizeof(Color));

            for (int j = 0; j < size; j++)
                img0[j + nOfFilters] = img[j];

            /* Instead of re-gathering the image after each filter, every process fully calculates it's
            part, then sends the result for gather one time, that's why the extension is necessary. */
            for (int i = 0; i < nOfFilters; i++)
                applyFilterPNM(img0, filters[i], start, end, i);

            /* Gathering and reconstruction the image from each individual part. */

            // process 0 get's his own part
            for (int j = 0; j < size; j++)
                img[j] = img0[j + nOfFilters];

            // then waits for the others to send theirs
            for (int i = 1; i < nProcesses; i++)
            {
                start = i * ceil((double)height / nProcesses);
                end = fmin(height, (i + 1) * ceil((double)height / nProcesses));

                for (int j = start; j < end; j++)
                {
                    Color *row = malloc(width * sizeof(Color));
                    unsigned char *red = malloc(width * sizeof(unsigned char));
                    unsigned char *green = malloc(width * sizeof(unsigned char));
                    unsigned char *blue = malloc(width * sizeof(unsigned char));

                    // for colored images, we send and receive the image in it's individual color channels
                    MPI_Recv(red, width, MPI_UNSIGNED_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    MPI_Recv(green, width, MPI_UNSIGNED_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    MPI_Recv(blue, width, MPI_UNSIGNED_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    img[j] = unpackColor(red, green, blue, width);
                }
            }
            // finally the constructed image is written
            writeImagePNM(img, outName);
        }
        /* Exact same logic for grayscale images. Only less processing. */
        else if (strstr(inName, ".pgm") > 0)
        {
            isColored = 0;
            unsigned char **img = readImagePGM(inName);
            int start, end;

            for (int p = 1; p < nProcesses; p++)
            {
                MPI_Send(&width, 1, MPI_INT, p, 0, MPI_COMM_WORLD);
                MPI_Send(&height, 1, MPI_INT, p, 0, MPI_COMM_WORLD);
                MPI_Send(&isColored, 1, MPI_INT, p, 0, MPI_COMM_WORLD);

                start = p * ceil((double)height / nProcesses);
                end = fmin(height, (p + 1) * ceil((double)height / nProcesses));

                for (int j = start - nOfFilters; j < end + nOfFilters; j++)
                    if (j < height)
                        MPI_Send(img[j], width, MPI_UNSIGNED_CHAR, p, 0, MPI_COMM_WORLD);
            }

            start = 0 * ceil((double)height / nProcesses);
            end = fmin(height, (0 + 1) * ceil((double)height / nProcesses) + nOfFilters);
            int size = end - start;

            unsigned char **img0 = (unsigned char **)calloc(size + 2 * nOfFilters, sizeof(unsigned char *));
            for (int j = 0; j < size + 2 * nOfFilters; j++)
                img0[j] = calloc(width, sizeof(unsigned char));

            for (int j = 0; j < size; j++)
                img0[j + nOfFilters] = img[j];

            for (int i = 0; i < nOfFilters; i++)
                applyFilterPGM(img0, filters[i], start, end, i);

            for (int j = 0; j < size; j++)
                img[j] = img0[j + nOfFilters];

            for (int i = 1; i < nProcesses; i++)
            {
                start = i * ceil((double)height / nProcesses);
                end = fmin(height, (i + 1) * ceil((double)height / nProcesses));

                for (int j = start; j < end; j++)
                {
                    unsigned char *row = malloc(width * sizeof(unsigned char));
                    MPI_Recv(row, width, MPI_UNSIGNED_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    img[j] = row;
                }
            }
            writeImagePGM(img, outName);
        }
        else
        {
            printf("Invalid format!");
            exit(0);
        }
    }
    /* Receiving processes part. */
    else
    {
        MPI_Recv(&width, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&height, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&isColored, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        int start, end;
        start = rank * ceil((double)height / nProcesses);
        end = fmin(height, (rank + 1) * ceil((double)height / nProcesses));

        int size = end - start;

        if (isColored)
        {
            // it awaits an extended part of the image
            Color **img = (Color **)calloc(size + 2 * nOfFilters, sizeof(Color *));
            for (int j = 0; j < size + 2 * nOfFilters; j++)
            {
                unsigned char *red = calloc(width, sizeof(unsigned char));
                unsigned char *green = calloc(width, sizeof(unsigned char));
                unsigned char *blue = calloc(width, sizeof(unsigned char));

                if (!(rank == nProcesses - 1 && j >= size + nOfFilters))
                {
                    MPI_Recv(red, width + 2, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    MPI_Recv(green, width + 2, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    MPI_Recv(blue, width + 2, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                }
                img[j] = unpackColor(red, green, blue, width);
            }
            // then it starts processing it
            for (int i = 0; i < nOfFilters; i++)
                applyFilterPNM(img, filters[i], start, end, i);
            // and sends it back to master
            for (int j = nOfFilters; j < size + nOfFilters; j++)
            {
                MPI_Send(packRedChannel(img[j], width), width, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD);
                MPI_Send(packGreenChannel(img[j], width), width, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD);
                MPI_Send(packBlueChannel(img[j], width), width, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD);
            }
        }
        else // identical logic for grayscale
        {
            unsigned char **img = (unsigned char **)calloc(size + 2 * nOfFilters, sizeof(unsigned char *));
            for (int j = 0; j < size + 2 * nOfFilters; j++)
            {
                unsigned char *row = calloc(width, sizeof(unsigned char));
                if (!(rank == nProcesses - 1 && j >= size + nOfFilters))
                    MPI_Recv(row, width, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                img[j] = row;
            }
            for (int i = 0; i < nOfFilters; i++)
                applyFilterPGM(img, filters[i], start, end, i);

            for (int j = nOfFilters; j < size + nOfFilters; j++)
                MPI_Send(img[j], width, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD);
        }
    }
    MPI_Finalize();
    return 0;
}
