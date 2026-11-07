# Pinned revisions of htmltads' sibling checkouts.
#
# htmltads is built as a sibling of tads-runner (the top-level CMake
# project), but only htmltads is tagged for a release. dependencies.txt, next
# to this folder, records the exact tads-runner commit that belongs to this
# htmltads commit, so a release tag fully determines what gets built:
#
#   - the GitHub workflows check out those commits (build.yml, release.yml)
#   - configuring checks the sibling checkouts against them
#     (htmltads_check_pinned_deps(), called from ../CMakeLists.txt): a warning
#     by default, an error with -DHTMLTADS_REQUIRE_PINNED_DEPS=ON
#   - deps.cmake records the current commits (update) or checks them out
#     (checkout), see there
#
# dependencies.txt holds one "name=full-commit-sha" line per sibling; lines
# starting with # are comments. The names are the sibling directory names.

set(HTMLTADS_DEPS_FILE "${CMAKE_CURRENT_LIST_DIR}/../dependencies.txt")
set(HTMLTADS_DEPS_NAMES tads-runner)

# Read dependencies.txt: sets HTMLTADS_PIN_<name> for each entry.
function(htmltads_read_pins)
	if (NOT EXISTS "${HTMLTADS_DEPS_FILE}")
		message(FATAL_ERROR "${HTMLTADS_DEPS_FILE} is missing")
	endif()
	file(STRINGS "${HTMLTADS_DEPS_FILE}" lines)
	foreach (line IN LISTS lines)
		if (line MATCHES "^[ \t]*([A-Za-z0-9_.-]+)[ \t]*=[ \t]*([0-9a-fA-F]+)[ \t]*$")
			set(HTMLTADS_PIN_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" PARENT_SCOPE)
		elseif (NOT line MATCHES "^[ \t]*(#.*)?$")
			message(FATAL_ERROR "${HTMLTADS_DEPS_FILE}: cannot parse line '${line}'")
		endif()
	endforeach()
endfunction()

# Run git in a checkout: sets <out_var> to its trimmed output, and
# <out_var>_RESULT to its exit code.
function(htmltads_git dir out_var)
	execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${dir}" ${ARGN}
		OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE res
		OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE)
	if (NOT res EQUAL 0 AND err)
		set(out "${err}")
	endif()
	set(${out_var} "${out}" PARENT_SCOPE)
	set(${out_var}_RESULT "${res}" PARENT_SCOPE)
endfunction()

# Describe a sibling checkout: sets <prefix>_HEAD (full sha, empty if <dir>
# isn't a git checkout) and <prefix>_DIRTY (the modified tracked files, as
# git status prints them; untracked files don't count).
function(htmltads_describe_checkout dir prefix)
	set(${prefix}_HEAD "" PARENT_SCOPE)
	set(${prefix}_DIRTY "" PARENT_SCOPE)
	if (NOT GIT_EXECUTABLE OR NOT EXISTS "${dir}/.git")
		return()
	endif()
	htmltads_git("${dir}" head rev-parse HEAD)
	if (NOT head_RESULT EQUAL 0)
		return()
	endif()
	htmltads_git("${dir}" dirty status --porcelain --untracked-files=no)
	set(${prefix}_HEAD "${head}" PARENT_SCOPE)
	set(${prefix}_DIRTY "${dirty}" PARENT_SCOPE)
endfunction()

# Configure-time check: are the sibling checkouts at the pinned commits?
# <tads_runner_dir> is the actual tads-runner folder.
function(htmltads_check_pinned_deps tads_runner_dir)
	option(HTMLTADS_REQUIRE_PINNED_DEPS
		"Fail the configure unless tads-runner is a clean checkout of the commit in htmltads/dependencies.txt (release builds)"
		OFF)
	if (HTMLTADS_REQUIRE_PINNED_DEPS)
		set(level FATAL_ERROR)
	else()
		set(level WARNING)
	endif()

	find_package(Git QUIET)
	htmltads_read_pins()
	set(HTMLTADS_DIR_tads-runner "${tads_runner_dir}")

	set(problems "")
	foreach (name IN LISTS HTMLTADS_DEPS_NAMES)
		set(dir "${HTMLTADS_DIR_${name}}")
		set(pin "${HTMLTADS_PIN_${name}}")
		if (NOT dir)
			continue()
		endif()
		if (NOT pin)
			string(APPEND problems "\n  ${name}: no entry in dependencies.txt")
			continue()
		endif()
		htmltads_describe_checkout("${dir}" co)
		if (NOT co_HEAD)
			string(APPEND problems "\n  ${name}: ${dir} is not a git checkout, cannot check it against ${pin}")
			continue()
		endif()
		string(TOLOWER "${pin}" pin)
		string(LENGTH "${pin}" pin_len)
		set(head_prefix "")
		if (pin_len GREATER_EQUAL 7 AND pin_len LESS_EQUAL 40)
			string(SUBSTRING "${co_HEAD}" 0 ${pin_len} head_prefix)
		endif()
		if (NOT head_prefix STREQUAL pin)
			string(SUBSTRING "${co_HEAD}" 0 12 short_head)
			string(APPEND problems "\n  ${name}: checked out ${short_head}, pinned ${pin}")
		endif()
		if (co_DIRTY)
			string(APPEND problems "\n  ${name}: has uncommitted changes to tracked files")
		endif()
	endforeach()

	if (problems)
		message(${level}
			"The tads-runner checkout doesn't match htmltads/dependencies.txt:${problems}\n"
			"Fine while developing. Before tagging a release, commit and push it and run "
			"'cmake -P htmltads/cmake/deps.cmake update'; to build exactly the pinned "
			"commits, run 'cmake -P htmltads/cmake/deps.cmake checkout'.")
	endif()
endfunction()
