# Test two SHARK models for equality based upon their constituent galaxies
#
# Copyright by Kai Striega
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#

import argparse

import h5py
from numpy.testing import assert_equal, assert_array_equal, assert_raises


def read_args():
    """Return an argparse.Namespace object with the CLI arguments"""
    arg_parser = argparse.ArgumentParser(
        "Test whether two SCHARK models are equivalent."
        )
    arg_parser.add_argument(
        '-e', '--expect-unequal', action='store_true',
        help='Whether the models are expected to be unequal.')
    arg_parser.add_argument(
        '-m', '--models', required=True, nargs=2,
        help='Path where each model is found.'
        )
    return arg_parser.parse_args()


def assert_galaxies_equal(galaxies1, galaxies2):
    assert_equal(galaxies1.keys(), galaxies2.keys())
    for key in galaxies1.keys():
        assert_array_equal(galaxies1[key][:], galaxies2[key][:])


def assert_galaxies_not_equal(galaxies1, galaxies2):
    assert_raises(AssertionError, assert_galaxies_equal, model1, model2)


def main():
    args = read_args()
    model_one = h5py.File(args.models[0])
    model_two = h5py.File(args.models[1])

    galaxies_one = model_one['galaxies']
    galaxies_two = model_two['galaxies']

    if args.expect_unequal:
        assert_galaxies_not_equal(galaxies_one, galaxies_two)
    else:
        assert_galaxies_equal(galaxies_one, galaxies_two)

if __name__ == '__main__':
    main()
