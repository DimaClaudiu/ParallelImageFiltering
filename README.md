# Parallel Image Filtering
Distributed colored and grayscale image filtering in C using MPI.

#### - Project Status: [Completed]

### Built with
  * C
  * [MPI](https://mpitutorial.com/tutorials/)
  
## Project Description
  The program aims to apply different styles of filters (smooth, blur, sharpen, mean, emboss) on a image, both rgb and grayscale are supported.
  It multiplies the rotated kernel of each filter with every pixel and it's neighbors. Multiple filters can be applied at once, any number in fact.
  
  To avoid unnecessary sends and receives that add overhead, the image is split and gathered back only once, and each process finishes it's full job before sending back it's part to be reconstructed. This is done by sending a padded segment of the image initially, the extra padding being based on the number of filters that need be applied. Thus the final concatenated image doesn't suffer from seams, and the execution time is blistering fast with minimal communication between processes.
  
  The scaling is really good with multiple filters, since the only overhead is done but once, no matter the number of filters.
  
### Examples
<img src="https://i.imgur.com/PK8R9wP.png" width="400">  <img src="https://i.imgur.com/izS5xU4.png" width="400"> 


<img src="https://i.imgur.com/bwuZNiQ.png" width="400">  <img src="https://i.imgur.com/oEYx6ea.png" width="400"> 


<img src="https://i.imgur.com/FdBEz12.png" width="400">  <img src="https://i.imgur.com/UH2GJva.png" width="400"> 


  
#### Time and CPU Usage
Average of 3 runs per test, 10 filters total (twice each).
> 3840x2160 8bit rgb image, 24MB size

| Cores       | Time (s)      | CPU Usage |
| ------------- |:-------------:| -----:|
| 1 Core      | 18.213 | 99%
| 2 Cores       | 10.237      |   187% |
| 3 Cores  | 7.69  |    257% |
| 4 Cores  | 6.373  |    326% |


> 3853x2000 8bit grayscale image, 7.5MB size

| Cores       | Time (s)      | CPU Usage |
| ------------- |:-------------:| -----:|
| 1 Core      |7.650 | 98%
| 2 Cores       | 4.256      |   186% |
| 3 Cores  | 3.186  |    257% |
| 4 Cores  | 2.630  |    326% |

    
The scalability is really good for more than one filter, and it just gets better with each addition.

## Usage
  Compile with `mpicc` and `-lm` flas.
  Run with command line arguments: `inputImage outputImage (smooth|blur|sharpen|mean|emboss)*`
  
  `mpirun -np 4 imageProcessing baby-yoda.pnm baby-yoda-filtered.pnm smooth blur sharpen mean emboss`

## Completion Date
2020, January 6
