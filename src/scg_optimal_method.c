/*
 *  SCGlib : A C library for the spectral coarse graining of matrices
 *	as described in the paper: Shrinking Matrices while preserving their
 *	eigenpairs with Application to the Spectral Coarse Graining of Graphs.
 *	Preprint available at <http://people.epfl.ch/david.morton>
 *  
 *	Copyright (C) 2008 David Morton de Lachapelle <david.morton@a3.epfl.ch>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 
 *  02110-1301 USA
 *
 *  DESCRIPTION
 *	-----------
 *    This file implements algorithm 5.8 of the above reference.
 *	  The optimal_partition function returns the minimizing partition
 *	  with size 'nt' of the objective function ||v-Pv||, where P is
 *	  a problem-specific projector. So far, Symmetric (matrix=1),
 *	  Laplacian (matrix=2) and Stochastic (matrix=3) projectors
 *	  have been implemented (the cost_matrix function below).
 *	  In the stochastic case, 'p' is expected to be a valid propability
 *	  vector. In all other cases, 'p' is ignored and can be set to NULL.
 *	  The group labels are given in 'gr' as positive consecutive integers
 *	  starting from 0.
 */

#include "igraph_error.h"
 
#include "scg_headers.h"

REAL igraph_i_optimal_partition(const REAL *v, UINT *gr,const UINT n,const UINT nt, 
										const UINT matrix, const REAL *p)
{
	/*-----------------------------------------------
	-----Sorts v and counts non-ties-----------------
	-----------------------------------------------*/
	UINT i, non_ties;
	INDVAL *vs = (INDVAL*)CALLOC(n, sizeof(INDVAL));
	
	for(i=0; i<n; i++){
		vs[i].val = v[i];
		vs[i].ind = i;
	}

	qsort(vs, n, sizeof(INDVAL), igraph_i_compare_ind_val);
	
	non_ties = 1;
	for(i=1; i<n; i++)
		if(vs[i].val != vs[i-1].val) non_ties++;

	if(nt >= non_ties){
		FREE(vs);
		igraph_error("when the optimal method is chosen, values in 'nt' must "//
			     "be smaller than the number of unique values in 'v'", __FILE__, __LINE__, IGRAPH_EINVAL);
	}
	
	//if stochastic SCG orders p
	REAL *ps = NULL;
	if(matrix==3){
		ps = igraph_i_real_vector(n);
		for(i=0; i<n; i++)
			ps[i] = p[vs[i].ind];
	}
	/*------------------------------------------------
	------Computes Cv, the matrix of costs------------
	------------------------------------------------*/
	REAL *Cv = igraph_i_real_sym_matrix(n);
	igraph_i_cost_matrix(Cv, vs, n, matrix, ps);
	if(matrix==3)
		igraph_i_free_real_vector(ps);
	/*-------------------------------------------------
	-------Fills up matrices F and Q-------------------
	-------------------------------------------------*/					
	UINT q;
	int j;
	/*here j also is a counter but the use of unsigned variables
	is to be proscribed in "for(unsigned int j=...;j>=0;j--)",
	for such loops never ends!*/
	REAL **F = igraph_i_real_matrix(nt,n);
	UINT **Q = igraph_i_uint_matrix(nt,n);
	REAL temp;
						
	for(i=0; i<n; i++) Q[0][i]++;
	for(i=0; i<nt; i++) Q[i][i]=i+1;
	
	for(i=0; i<n; i++)
		F[0][i] = igraph_i_real_sym_mat_get(Cv,0,i);
		
	for(i=1; i<nt; i++)
		for(j=i+1; j<n; j++){
			F[i][j] = F[i-1][i-1] + igraph_i_real_sym_mat_get(Cv,i,j);
			Q[i][j] = 2;
		
			for(q=i-1; q<=j-1; q++){
				temp = F[i-1][q] + igraph_i_real_sym_mat_get(Cv,q+1,j);
				if(temp<F[i][j]){
					F[i][j] = temp;
					Q[i][j] = q+2;
				}
			}
		}
	igraph_i_free_real_sym_matrix(Cv);
	/*--------------------------------------------------
	-------Back-tracks through Q to work out the groups-
	--------------------------------------------------*/
	UINT l;
	UINT part_ind = nt;
	UINT col = n-1;
	REAL sumOfSquares;

	for(j=nt-1; j>=0; j--){
		for(i=Q[j][col]-1; i<=col; i++)
			gr[vs[i].ind] = part_ind-1 + FIRST_GROUP_NB;
		if(Q[j][col] != 2){
			col = Q[j][col]-2;
			part_ind -= 1;
		}
		else{
			if(j>1){
				for(l=0; l<=(j-1); l++)
					gr[vs[l].ind] = l + FIRST_GROUP_NB;
				break;
			}
			else{
				col = Q[j][col]-2;
				part_ind -= 1;
			}
		}
	}
	
	sumOfSquares = F[nt-1][n-1];  

	igraph_i_free_uint_matrix(Q,nt);
	igraph_i_free_real_matrix(F,nt);
	FREE(vs);

	return sumOfSquares;
}

void igraph_i_cost_matrix(REAL*Cv, const INDVAL *vs, const UINT n, const UINT matrix, const REAL *ps)
{
	//if symmetric of Laplacian SCG -> same Cv
	if(matrix==1 || matrix==2){
		UINT i,j;
		REAL *w  = igraph_i_real_vector(n+1);
		REAL *w2 = igraph_i_real_vector(n+1);
	
		w[1] = vs[0].val;
		w2[1] = vs[0].val*vs[0].val;
	
		for(i=2; i<=n; i++){
			w[i] = w[i-1] + vs[i-1].val;
			w2[i] = w2[i-1] + vs[i-1].val*vs[i-1].val;
		}
	
		for(i=0; i<n; i++)
			for(j=i+1; j<n; j++)
				igraph_i_real_sym_mat_set(Cv,i,j,
							(w2[j+1]-w2[i])-(w[j+1]-w[i])*(w[j+1]-w[i])/(j-i+1) );		
		igraph_i_free_real_vector(w);
		igraph_i_free_real_vector(w2);
	}
	//if stochastic
	//TODO: optimize it to O(n^2) instead of O(n^3) (as above)
	if(matrix==3){
		UINT i,j,k;
		REAL t1,t2;
		for(i=0; i<n; i++){
			for(j=i+1; j<n; j++){
				t1 = t2 = 0;
				for(k=i; k<j; k++){
					t1 += ps[k];
					t2 += ps[k]*vs[k].val;
				}
				t1 = t2/t1;
				t2 = 0;
				for(k=i; k<j; k++)
					t2 += (vs[k].val-t1)*(vs[k].val-t1);
				igraph_i_real_sym_mat_set(Cv,i,j,t2);
			}
		}
	}
	
}

