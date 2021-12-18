// Copyright 2010-2021 Google LLC
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

// This file solves a 2D Bin Packing problem.
// It loads the size of the main rectangle, all available items (rectangles
// too), and tries to fit all rectangles in the minimum numbers of bins (they
// have the size of the main rectangle.)

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/packing/binpacking_2d_parser.h"
#include "ortools/packing/multiple_dimensions_bin_packing.pb.h"
#include "ortools/sat/cp_model.h"

ABSL_FLAG(std::string, input, "", "Input file.");
ABSL_FLAG(int, instance, -1, "Instance number if the file.");
ABSL_FLAG(std::string, params, "", "Sat parameters in text proto format.");
ABSL_FLAG(int, max_bins, 0,
          "Maximum number of bins. The 0 default value implies the code will "
          "use some heuristics to compute this number.");

namespace operations_research {
namespace sat {

// Load a 2D binpacking problem and solve it.
void LoadAndSolve(const std::string& file_name, int instance) {
  packing::BinPacking2dParser parser;
  if (!parser.Load2BPFile(file_name, instance)) {
    LOG(FATAL) << "Cannot read instance " << instance << " from file "
               << file_name;
  }
  packing::MultipleDimensionsBinPackingProblem problem = parser.problem();
  LOG(INFO) << "Successfully loaded instance " << instance << " from file "
            << file_name;
  LOG(INFO) << "Instance has " << problem.items_size() << " items";

  const auto box_dimensions = problem.box_shape().dimensions();
  const int num_dimensions = box_dimensions.size();
  const int num_items = problem.items_size();

  const int area_of_one_bin = box_dimensions[0] * box_dimensions[1];
  int sum_of_items_area = 0;
  for (const auto& item : problem.items()) {
    CHECK_EQ(1, item.shapes_size());
    const auto& shape = item.shapes(0);
    CHECK_EQ(2, shape.dimensions_size());
    sum_of_items_area += shape.dimensions(0) * shape.dimensions(1);
  }
  const int trivial_lb =
      (sum_of_items_area + area_of_one_bin - 1) / area_of_one_bin;

  LOG(INFO) << "Trivial lower bound of the number of items = " << trivial_lb;
  if (absl::GetFlag(FLAGS_max_bins) == 0) {
    LOG(INFO) << "Setting max_bins to " << trivial_lb * 2;
  }
  const int max_bins = absl::GetFlag(FLAGS_max_bins) == 0
                           ? trivial_lb * 2
                           : absl::GetFlag(FLAGS_max_bins);

  CpModelBuilder cp_model;

  // Selects the right shape for each item (plus nil shape if not selected).
  // The nil shape is the first choice.
  std::vector<std::vector<BoolVar>> selected(num_items);
  for (int item = 0; item < num_items; ++item) {
    const int num_shapes = problem.items(item).shapes_size();
    CHECK_EQ(1, num_shapes);
    selected[item].resize(max_bins);
    for (int b = 0; b < max_bins; ++b) {
      selected[item][b] = cp_model.NewBoolVar();
    }
  }

  // Exactly one bin is selected for each item.
  for (int item = 0; item < num_items; ++item) {
    cp_model.AddEquality(LinearExpr::Sum(selected[item]), 1);
  }

  // Manages positions and sizes for each item.
  std::vector<std::vector<std::vector<IntervalVar>>> intervals(num_items);
  for (int item = 0; item < num_items; ++item) {
    intervals[item].resize(max_bins);
    for (int b = 0; b < max_bins; ++b) {
      intervals[item][b].resize(2);
      for (int dim = 0; dim < num_dimensions; ++dim) {
        const int64_t dimension = box_dimensions[dim];
        const int64_t size = problem.items(item).shapes(0).dimensions(dim);
        IntVar start = cp_model.NewIntVar({0, dimension - size});
        intervals[item][b][dim] = cp_model.NewOptionalFixedSizeIntervalVar(
            start, size, selected[item][b]);
      }
    }
  }

  // Non overlapping.
  if (num_dimensions == 1) {
    LOG(FATAL) << "One dimension is not supported.";
  } else if (num_dimensions == 2) {
    LOG(INFO) << "Box size: " << box_dimensions[0] << "*" << box_dimensions[1];
    for (int b = 0; b < max_bins; ++b) {
      NoOverlap2DConstraint no_overlap_2d = cp_model.AddNoOverlap2D();
      for (int item = 0; item < num_items; ++item) {
        no_overlap_2d.AddRectangle(intervals[item][b][0],
                                   intervals[item][b][1]);
      }
    }
  } else {
    LOG(FATAL) << num_dimensions << " dimensions not supported.";
  }

  // Redundant constraint.
  LinearExpr sum_of_areas;
  for (int item = 0; item < num_items; ++item) {
    const int item_area = problem.items(item).shapes(0).dimensions(0) *
                          problem.items(item).shapes(0).dimensions(1);
    for (int b = 0; b < max_bins; ++b) {
      sum_of_areas += selected[item][b] * item_area;
    }
  }
  cp_model.AddEquality(sum_of_areas, sum_of_items_area);

  // Symmetry breaking: The number of items per bin is decreasing.
  std::vector<LinearExpr> num_items_per_bins(max_bins);
  LinearExpr all_items;
  for (int b = 0; b < max_bins; ++b) {
    for (int item = 0; item < num_items; ++item) {
      num_items_per_bins[b] += selected[item][b];
      all_items += selected[item][b];
    }
  }
  for (int b = 1; b < max_bins; ++b) {
    cp_model.AddLessOrEqual(num_items_per_bins[b - 1], num_items_per_bins[b]);
  }
  cp_model.AddEquality(all_items, num_items);

  // Objective.
  std::vector<BoolVar> bin_is_selected(max_bins);
  for (int b = 0; b < max_bins; ++b) {
    bin_is_selected[b] = cp_model.NewBoolVar();
    // Link bin_is_selected[i] with the items in bin i.
    std::vector<BoolVar> all_items;
    for (int item = 0; item < num_items; ++item) {
      cp_model.AddImplication(selected[item][b], bin_is_selected[b]);
      all_items.push_back(selected[item][b].Not());
    }
    all_items.push_back(bin_is_selected[b].Not());
    cp_model.AddBoolOr(all_items);
  }
  cp_model.Minimize(LinearExpr::Sum(bin_is_selected));

  // Setup parameters.
  SatParameters parameters;
  parameters.set_log_search_progress(true);
  // Parse the --params flag.
  if (!absl::GetFlag(FLAGS_params).empty()) {
    CHECK(google::protobuf::TextFormat::MergeFromString(
        absl::GetFlag(FLAGS_params), &parameters))
        << absl::GetFlag(FLAGS_params);
  }

  const CpSolverResponse response =
      SolveWithParameters(cp_model.Build(), parameters);
}

}  // namespace sat
}  // namespace operations_research

int main(int argc, char** argv) {
  absl::SetFlag(&FLAGS_logtostderr, true);
  google::InitGoogleLogging(argv[0]);
  absl::ParseCommandLine(argc, argv);
  if (absl::GetFlag(FLAGS_input).empty()) {
    LOG(FATAL) << "Please supply a data file with --input=";
  }
  if (absl::GetFlag(FLAGS_instance) == -1) {
    LOG(FATAL) << "Please supply a valid instance number with --instance=";
  }

  operations_research::sat::LoadAndSolve(absl::GetFlag(FLAGS_input),
                                         absl::GetFlag(FLAGS_instance));
  return EXIT_SUCCESS;
}
