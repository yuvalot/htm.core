# -----------------------------------------------------------------------------
# HTM Community Edition of NuPIC
# Copyright (C) 2016, Numenta, Inc.
# modified 4/4/2022 - newer version
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU Affero Public License for more details.
#
# You should have received a copy of the GNU Affero Public License
# along with this program.  If not, see http://www.gnu.org/licenses.
# -----------------------------------------------------------------------------

# Fetch Eigen from GitHub archive
#
if(EXISTS "${REPOSITORY_DIR}/build/ThirdParty/share/eigen.tar.gz")
    set(URL "${REPOSITORY_DIR}/build/ThirdParty/share/eigen.tar.gz")
else()
	set(URL "https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz")
endif()

message(STATUS "obtaining Eigen")
include(DownloadProject/DownloadProject.cmake)
download_project(PROJ eigen
	PREFIX ${EP_BASE}/eigen
	URL ${URL}
	UPDATE_DISCONNECTED 1
	QUIET
	)
	
# No build. This is a header only package


FILE(APPEND "${EXPORT_FILE_NAME}" "eigen_INCLUDE_DIRS@@@${eigen_SOURCE_DIR}\n")
