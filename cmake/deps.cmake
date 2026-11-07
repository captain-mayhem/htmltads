# Record or check out the tads-runner commit pinned in htmltads/dependencies.txt
# (see HtmltadsDeps.cmake). Run from anywhere:
#
#   cmake -P htmltads/cmake/deps.cmake update
#       Write the current commit of ../tads-runner to
#       dependencies.txt. Do this before tagging an htmltads release, then
#       commit dependencies.txt. Refuses if tads-runner has uncommitted changes
#       to tracked files, or its commit isn't on any remote branch (pushed),
#       since then CI couldn't check it out.
#
#   cmake -P htmltads/cmake/deps.cmake checkout
#       Check out the pinned commit in ../tads-runner (as a detached HEAD),
#       e.g. to rebuild an older htmltads release. Fetches if a commit isn't
#       there yet. Refuses if tads-runner has uncommitted changes to tracked
#       files. Afterwards, 'git switch master' there goes back.
#
#   cmake -P htmltads/cmake/deps.cmake status
#       Show the pinned and the checked-out commit.
#
# -DTADS_RUNNER_DIR=... (before -P) uses another tads-runner folder.

cmake_minimum_required(VERSION 3.17)
include("${CMAKE_CURRENT_LIST_DIR}/HtmltadsDeps.cmake")

get_filename_component(htmltads_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if (NOT TADS_RUNNER_DIR)
	set(TADS_RUNNER_DIR "${htmltads_root}/../tads-runner")
endif()
get_filename_component(HTMLTADS_DIR_tads-runner "${TADS_RUNNER_DIR}" ABSOLUTE)

# the action is the argument after "-P <this script>"
set(action "")
math(EXPR last "${CMAKE_ARGC} - 1")
foreach (i RANGE ${last})
	if (CMAKE_ARGV${i} STREQUAL "-P")
		math(EXPR a "${i} + 2")
		if (a LESS CMAKE_ARGC)
			set(action "${CMAKE_ARGV${a}}")
		endif()
		break()
	endif()
endforeach()

find_package(Git QUIET)
if (NOT GIT_EXECUTABLE)
	message(FATAL_ERROR "git not found")
endif()

if (action STREQUAL "update")
	set(content
"# Commit of the tads-runner sibling checkout that belongs to this htmltads commit.
# Only htmltads is tagged for a release; the workflows check it out, and
# configuring checks the local checkout against it. Update with
# 'cmake -P htmltads/cmake/deps.cmake update' - see cmake/HtmltadsDeps.cmake.
")
	set(errors "")
	foreach (name IN LISTS HTMLTADS_DEPS_NAMES)
		set(dir "${HTMLTADS_DIR_${name}}")
		htmltads_describe_checkout("${dir}" co)
		if (NOT co_HEAD)
			string(APPEND errors "\n  ${name}: ${dir} is not a git checkout")
			continue()
		endif()
		if (co_DIRTY)
			string(APPEND errors "\n  ${name}: uncommitted changes to tracked files - commit them first:\n${co_DIRTY}")
		endif()
		htmltads_git("${dir}" remotes branch -r --contains HEAD)
		if (NOT remotes_RESULT EQUAL 0 OR remotes STREQUAL "")
			string(APPEND errors "\n  ${name}: ${co_HEAD} is on no remote branch - push it first")
		endif()
		string(APPEND content "${name}=${co_HEAD}\n")
		message(STATUS "${name}: ${co_HEAD}")
	endforeach()
	if (errors)
		message(FATAL_ERROR "dependencies.txt not updated:${errors}")
	endif()
	file(WRITE "${HTMLTADS_DEPS_FILE}" "${content}")
	message(STATUS "Wrote ${HTMLTADS_DEPS_FILE} - commit it with the release.")

elseif (action STREQUAL "checkout")
	htmltads_read_pins()
	# check everything first, so nothing is checked out halfway
	foreach (name IN LISTS HTMLTADS_DEPS_NAMES)
		set(dir "${HTMLTADS_DIR_${name}}")
		if (NOT HTMLTADS_PIN_${name})
			message(FATAL_ERROR "${name}: no entry in dependencies.txt")
		endif()
		htmltads_describe_checkout("${dir}" co)
		if (NOT co_HEAD)
			message(FATAL_ERROR "${name}: ${dir} is not a git checkout")
		endif()
		if (co_DIRTY)
			message(FATAL_ERROR "${name}: uncommitted changes to tracked files - commit or stash them first:\n${co_DIRTY}")
		endif()
	endforeach()
	foreach (name IN LISTS HTMLTADS_DEPS_NAMES)
		set(dir "${HTMLTADS_DIR_${name}}")
		set(pin "${HTMLTADS_PIN_${name}}")
		htmltads_git("${dir}" have cat-file -e "${pin}^{commit}")
		if (NOT have_RESULT EQUAL 0)
			message(STATUS "${name}: fetching")
			htmltads_git("${dir}" out fetch origin)
			if (NOT out_RESULT EQUAL 0)
				message(FATAL_ERROR "${name}: git fetch failed:\n${out}")
			endif()
		endif()
		htmltads_git("${dir}" out -c advice.detachedHead=false checkout --detach "${pin}")
		if (NOT out_RESULT EQUAL 0)
			message(FATAL_ERROR "${name}: cannot check out ${pin}:\n${out}")
		endif()
		message(STATUS "${name}: checked out ${pin}")
	endforeach()

elseif (action STREQUAL "status")
	htmltads_read_pins()
	foreach (name IN LISTS HTMLTADS_DEPS_NAMES)
		htmltads_describe_checkout("${HTMLTADS_DIR_${name}}" co)
		set(state "")
		if (co_DIRTY)
			set(state " (uncommitted changes)")
		endif()
		message(STATUS "${name}:\n     pinned:      ${HTMLTADS_PIN_${name}}\n     checked out: ${co_HEAD}${state}")
	endforeach()

else()
	message(FATAL_ERROR "usage: cmake -P ${CMAKE_CURRENT_LIST_FILE} update|checkout|status")
endif()
