/**
 *
 * RV-GOMEA
 *
 * If you use this software for any purpose, please cite the most recent publication:
 * A. Bouter, C. Witteveen, T. Alderliesten, P.A.N. Bosman. 2017.
 * Exploiting Linkage Information in Real-Valued Optimization with the Real-Valued
 * Gene-pool Optimal Mixing Evolutionary Algorithm. In Proceedings of the Genetic 
 * and Evolutionary Computation Conference (GECCO 2017).
 * DOI: 10.1145/3071178.3071272
 *
 * Copyright (c) 1998-2017 Peter A.N. Bosman
 *
 * The software in this file is the proprietary information of
 * Peter A.N. Bosman.
 *
 * IN NO EVENT WILL THE AUTHOR OF THIS SOFTWARE BE LIABLE TO YOU FOR ANY
 * DAMAGES, INCLUDING BUT NOT LIMITED TO LOST PROFITS, LOST SAVINGS, OR OTHER
 * INCIDENTIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR THE INABILITY
 * TO USE SUCH PROGRAM, EVEN IF THE AUTHOR HAS BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGES, OR FOR ANY CLAIM BY ANY OTHER PARTY. THE AUTHOR MAKES NO
 * REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY OF THE SOFTWARE, EITHER
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT. THE
 * AUTHOR SHALL NOT BE LIABLE FOR ANY DAMAGES SUFFERED BY ANYONE AS A RESULT OF
 * USING, MODIFYING OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.
 *
 * The software in this file is the result of (ongoing) scientific research.
 * The following people have been actively involved in this research over
 * the years:
 * - Peter A.N. Bosman
 * - Dirk Thierens
 * - Jörn Grahl
 * - Anton Bouter
 * 
 */

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-= Section Includes -=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
#include "Optimization.h"
/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

/**
 * Computes the rotation matrix to be applied to any solution
 * before evaluating it (i.e. turns the evaluation functions
 * into rotated evaluation functions).
 */
void initializeObjectiveRotationMatrix( void )
{
    int      i, j, index0, index1;
    double **matrix, **product, theta, cos_theta, sin_theta;

    if( rotation_angle == 0.0 )
        return;

    matrix = (double **) Malloc( block_size*sizeof( double * ) );
    for( i = 0; i < block_size; i++ )
        matrix[i] = (double *) Malloc( block_size*sizeof( double ) );

    rotation_matrix = (double **) Malloc( block_size*sizeof( double * ) );
    for( i = 0; i < block_size; i++ )
        rotation_matrix[i] = (double *) Malloc( block_size*sizeof( double ) );

    /* Initialize the rotation matrix to the identity matrix */
    for( i = 0; i < block_size; i++ )
    {
        for( j = 0; j < block_size; j++ )
            rotation_matrix[i][j] = 0.0;
        rotation_matrix[i][i] = 1.0;
    }

    /* Construct all rotation matrices (quadratic number) and multiply */
    theta     = (rotation_angle/180.0)*PI;
    cos_theta = cos( theta );
    sin_theta = sin( theta );
    for( index0 = 0; index0 < block_size-1; index0++ )
    {
        for( index1 = index0+1; index1 < block_size; index1++ )
        {
            for( i = 0; i < block_size; i++ )
            {
                for( j = 0; j < block_size; j++ )
                    matrix[i][j] = 0.0;
                matrix[i][i] = 1.0;
            }
            matrix[index0][index0] = cos_theta;
            matrix[index0][index1] = -sin_theta;
            matrix[index1][index0] = sin_theta;
            matrix[index1][index1] = cos_theta;

            product = matrixMatrixMultiplication( matrix, rotation_matrix, block_size, block_size, block_size );
            for( i = 0; i < block_size; i++ )
                for( j = 0; j < block_size; j++ )
                    rotation_matrix[i][j] = product[i][j];

            for( i = 0; i < block_size; i++ )
                free( product[i] );
            free( product );
        }
    }

    for( i = 0; i < block_size; i++ )
        free( matrix[i] );
    free( matrix );
}

void ezilaitiniObjectiveRotationMatrix( void )
{
    int i;

    if( rotation_angle == 0.0 )
        return;

    for( i = 0; i < block_size; i++ )
        free( rotation_matrix[i] );
    free( rotation_matrix );
}

double *rotateAllParameters( double *parameters )
{
    return( rotateParametersInRange( parameters, 0, number_of_parameters-1 ) );
}

double *rotateParametersInRange( double *parameters, int from, int to )
{
    int i, j;
    double *rotated_parameters, *cluster, *rotated_cluster;

    rotated_parameters = (double*) Malloc( number_of_parameters*sizeof( double ) );
    for( i = 0; i < from; i++ ) rotated_parameters[i] = parameters[i];
    for( i = 0; i < number_of_blocks; i++ )
    {
        cluster = (double*) Malloc( block_size*sizeof( double ) );
        for( j = 0; j < block_size; j++ )
            cluster[j] = parameters[from + i*block_size + j];
        rotated_cluster = matrixVectorMultiplication( rotation_matrix, cluster, block_size, block_size );
        for( j = 0; j < block_size; j++ )
            rotated_parameters[from + i*block_size + j] = rotated_cluster[j];
        free( cluster );
        free( rotated_cluster );
    }
    for( i = to+1; i < number_of_parameters; i++ ) rotated_parameters[i] = parameters[i];
    return( rotated_parameters );
}
