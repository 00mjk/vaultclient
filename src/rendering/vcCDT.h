#ifndef vcCDT_h__
#define vcCDT_h__
/*
 * Poly2Tri Copyright (c) 2009-2018, Poly2Tri Contributors
 * https://github.com/jhasse/poly2tri
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of Poly2Tri nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without specific
 *   prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
* Constrained Delaunay Triangulation
* Modified by Asuna Wu 2020-2-18
*/

#include "vcMath.h"
#include <vector>

//Constrained Delaunay Triangulation
bool vcCDT_ProcessOrignal(const udDouble2 *pWaterPoints, size_t pointNum,
  const std::vector< std::pair<const udDouble2 *, size_t> > &islandPoints
  , udDouble2 &oMin
  , udDouble2 &oMax
  , udDouble4x4 &oOrigin
  , std::vector<udDouble2> *pResult);

// triangulate a contour/polygon, places results in STL vector as series of triangles.
bool vcCDT_Process(const udDouble2 *pWaterPoints, size_t pointNum,
  const std::vector< std::pair<const udDouble2*, size_t> > &islandPoints
  , udDouble2 &oMin
  , udDouble2 &oMax
  , udDouble4x4 &oOrigin
  , std::vector<udDouble2> *pResult);


#endif