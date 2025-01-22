#!/usr/bin/env python3
# Copyright 2010-2024 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from absl.testing import absltest
from ortools.math_opt import model_pb2
from ortools.math_opt.io.python import mps_converter
from ortools.math_opt.python import mathopt
from ortools.math_opt.python.testing import compare_proto

_MODEL_MPS = """* Generated by MPModelProtoExporter
*   Name             : unbounded_integers
*   Format           : Free
*   Constraints      : 1
*   Variables        : 2
*     Binary         : 0
*     Integer        : 2
*     Continuous     : 0
NAME          unbounded_integers
ROWS
 N  COST
 G  c
COLUMNS
  INTSTART  'MARKER'                            'INTORG'
    x       COST         1  c            1
    y       COST         1  c            1
  INTEND    'MARKER'                            'INTEND'
RHS
    RHS     c            2
BOUNDS
 LI BOUND   x            1
 LI BOUND   y            4
ENDATA
"""


def simple_model_proto() -> model_pb2.ModelProto:
    model = mathopt.Model(name="unbounded_integers")
    x = model.add_variable(name="x", lb=1, ub=float("inf"), is_integer=True)
    y = model.add_variable(name="y", lb=4, ub=float("inf"), is_integer=True)
    model.add_linear_constraint(x + y >= 2, name="c")
    model.minimize(x + y)
    return model.export_model()


class MPSConverterTest(absltest.TestCase, compare_proto.MathOptProtoAssertions):

    def test_convert_empty_mps_to_model_proto(self) -> None:
        simple_mps = "NAME MIN_SIZE_MAX_FEATURES"
        model_proto = mps_converter.mps_to_model_proto(simple_mps)
        self.assertEqual(model_proto.name, "MIN_SIZE_MAX_FEATURES")

    def test_convert_simple_mps_to_model(self) -> None:
        model_proto = mps_converter.mps_to_model_proto(_MODEL_MPS)
        expected_model_proto = simple_model_proto()

        self.assert_protos_equiv(model_proto, expected_model_proto)

    def test_convert_model_proto_to_mps(self) -> None:
        model_proto = simple_model_proto()
        mps = mps_converter.model_proto_to_mps(model_proto)
        self.assertEqual(mps, _MODEL_MPS)


if __name__ == "__main__":
    absltest.main()
