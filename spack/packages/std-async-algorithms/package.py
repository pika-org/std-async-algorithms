# Copyright 2013-2022 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack.package import *


class StdAsyncAlgorithms(CMakePackage):
    """TODO"""

    homepage = "https://github.com/pika-org/std-async-algorithms"
    git = "https://github.com/pika-org/std-async-algorithms.git"
    maintainers = ["aurianer", "msimberg"]

    version("main", branch="main")

    variant("pika", default=False, description="Depend on pika")

    depends_on("cmake@3.22:", type="build")

    depends_on("stdexec@c0fe16f451137b00ceb85d912ec3a288990ce766")
    depends_on("pika +p2300", when="+pika")
