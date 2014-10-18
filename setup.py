# -*- coding: utf-8 -*-
# Copyright © 2014 Casey Dahlin <casey.dahlin@gmail.com>
#
# This file is part of pyason.
#
# pyason is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# pyason is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with pyason. If not, see <http://www.gnu.org/licenses/>.

from distutils.core import setup, Extension

ason_module = Extension('ason',
        sources = ['asonmodule.c'],
        libraries = ['ason'])

setup(name = 'pyason',
      version = '0.1',
      author = 'Casey Dahlin',
      author_email = 'casey.dahlin@gmail.com',
      description = 'Library for manipulating ASON values',
      ext_modules = [ason_module]
     )

