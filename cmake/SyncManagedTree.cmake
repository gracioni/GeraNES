if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()

if(NOT DEFINED DEST_DIR)
    message(FATAL_ERROR "DEST_DIR is required")
endif()

if(NOT DEFINED MANIFEST_FILE)
    message(FATAL_ERROR "MANIFEST_FILE is required")
endif()

foreach(path_var SOURCE_DIR DEST_DIR MANIFEST_FILE)
    string(REGEX REPLACE "^\"(.*)\"$" "\\1" ${path_var} "${${path_var}}")
endforeach()

file(MAKE_DIRECTORY "${DEST_DIR}")
get_filename_component(manifest_dir "${MANIFEST_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${manifest_dir}")

set(previous_entries "")
if(EXISTS "${MANIFEST_FILE}")
    file(STRINGS "${MANIFEST_FILE}" previous_entries)
endif()

file(GLOB current_entries RELATIVE "${SOURCE_DIR}" "${SOURCE_DIR}/*")

set(entries_to_remove ${previous_entries} ${current_entries})
list(REMOVE_DUPLICATES entries_to_remove)

foreach(entry IN LISTS entries_to_remove)
    if(entry STREQUAL "")
        continue()
    endif()
    file(REMOVE_RECURSE "${DEST_DIR}/${entry}")
    file(REMOVE "${DEST_DIR}/${entry}")
endforeach()

foreach(entry IN LISTS current_entries)
    if(entry STREQUAL "")
        continue()
    endif()
    file(COPY "${SOURCE_DIR}/${entry}" DESTINATION "${DEST_DIR}")
endforeach()

list(SORT current_entries)
string(REPLACE ";" "\n" manifest_contents "${current_entries}")
if(manifest_contents STREQUAL "")
    file(WRITE "${MANIFEST_FILE}" "")
else()
    file(WRITE "${MANIFEST_FILE}" "${manifest_contents}\n")
endif()
