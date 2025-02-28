// Copyright 2010-2025 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// TODO(user): Refactor this file to adhere to the SWIG style guide.

%include "ortools/base/base.i"
%include "enums.swg"
%import "ortools/util/csharp/vector.i"

// Include the file we want to wrap a first time.
%{
#include "ortools/algorithms/knapsack_solver.h"
%}

// by default vector<vector<int64_t>> is mapped to a jagged array i.e. .Net type long[][]
// but here we want a regular matrix i.e. .Net type long[,]
%template(Int64Vector) std::vector<int64_t>;
%template(Int64Matrix) std::vector<std::vector<int64_t> >;
VECTOR_AS_CSHARP_ARRAY(int64_t, int64_t, long, Int64Vector);
REGULAR_MATRIX_AS_CSHARP_ARRAY(int64_t, int64_t, long, Int64Matrix);

namespace operations_research {

%unignore KnapsackSolver;
%rename (UseReduction) KnapsackSolver::use_reduction;
%rename (SetUseReduction) KnapsackSolver::set_use_reduction;

}  // namespace operations_research

// TODO(user): Replace with %ignoreall/%unignoreall
//swiglint: disable include-h-allglobals
%include "ortools/algorithms/knapsack_solver.h"
