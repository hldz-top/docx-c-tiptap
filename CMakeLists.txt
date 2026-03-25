cmake_minimum_required(VERSION 3.16)
project(docx2tiptap C)

# 若在添加 vcpkg.json 之前已生成过缓存，VCPKG_MANIFEST_MODE 会保持 OFF，依赖不会安装；vcpkg 也不允许在同一 build 目录上事后打开 manifest。
if(CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg\\.cmake$"
    AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/vcpkg.json"
    AND NOT VCPKG_MANIFEST_MODE)
    message(FATAL_ERROR
        "vcpkg.json 已存在，但本构建目录中 VCPKG_MANIFEST_MODE=OFF（多为旧 CMake 缓存）。"
        " libxml2 不会从清单安装，find_package(LibXml2) 会失败。\n"
        "请删除整个 build 目录后重新配置；不要沿用添加 vcpkg.json 之前的构建树。")
endif()

set(CMAKE_C_STANDARD 11)

find_package(LibXml2 CONFIG QUIET)
if(NOT LibXml2_FOUND)
    find_package(LibXml2 REQUIRED)
endif()

set(DOCX2TIPTAP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/docx2tiptap)

add_library(miniz_vendor STATIC
    ${DOCX2TIPTAP_ROOT}/vendor/miniz.c
    ${DOCX2TIPTAP_ROOT}/vendor/miniz_tdef.c
    ${DOCX2TIPTAP_ROOT}/vendor/miniz_tinfl.c
    ${DOCX2TIPTAP_ROOT}/vendor/miniz_zip.c
)
target_include_directories(miniz_vendor PUBLIC ${DOCX2TIPTAP_ROOT}/vendor)

add_library(yyjson_vendor STATIC ${DOCX2TIPTAP_ROOT}/vendor/yyjson.c)
target_include_directories(yyjson_vendor PUBLIC ${DOCX2TIPTAP_ROOT}/vendor)

set(DOCX2TIPTAP_SOURCES
    ${DOCX2TIPTAP_ROOT}/src/arena.c
    ${DOCX2TIPTAP_ROOT}/src/xml_util.c
    ${DOCX2TIPTAP_ROOT}/src/zip_reader.c
    ${DOCX2TIPTAP_ROOT}/src/parse_context.c
    ${DOCX2TIPTAP_ROOT}/src/rels_reader.c
    ${DOCX2TIPTAP_ROOT}/src/styles_reader.c
    ${DOCX2TIPTAP_ROOT}/src/numbering_reader.c
    ${DOCX2TIPTAP_ROOT}/src/doc_reader.c
    ${DOCX2TIPTAP_ROOT}/src/block_mapper.c
    ${DOCX2TIPTAP_ROOT}/src/inline_mapper.c
    ${DOCX2TIPTAP_ROOT}/src/table_mapper.c
    ${DOCX2TIPTAP_ROOT}/src/media_mapper.c
    ${DOCX2TIPTAP_ROOT}/src/json_writer.c
    ${DOCX2TIPTAP_ROOT}/src/api.c
)

add_library(docx2tiptap STATIC ${DOCX2TIPTAP_SOURCES})
target_include_directories(docx2tiptap
    PUBLIC
        ${DOCX2TIPTAP_ROOT}/include
    PRIVATE
        ${DOCX2TIPTAP_ROOT}/src
        ${LIBXML2_INCLUDE_DIR}
)
if(TARGET LibXml2::LibXml2)
    target_link_libraries(docx2tiptap PUBLIC miniz_vendor yyjson_vendor LibXml2::LibXml2)
else()
    target_include_directories(docx2tiptap PRIVATE ${LIBXML2_INCLUDE_DIR})
    target_link_libraries(docx2tiptap PUBLIC miniz_vendor yyjson_vendor ${LIBXML2_LIBRARIES})
endif()
if(WIN32 AND MSVC)
    target_compile_definitions(docx2tiptap PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_executable(docx2tiptap_cli ${DOCX2TIPTAP_ROOT}/cli/main.c)
target_link_libraries(docx2tiptap_cli PRIVATE docx2tiptap)

add_executable(test_arena ${DOCX2TIPTAP_ROOT}/tests/test_arena.c)
target_link_libraries(test_arena PRIVATE docx2tiptap)

add_executable(test_zip ${DOCX2TIPTAP_ROOT}/tests/test_zip.c)
target_link_libraries(test_zip PRIVATE docx2tiptap miniz_vendor)

add_executable(test_parse ${DOCX2TIPTAP_ROOT}/tests/test_parse.c)
target_link_libraries(test_parse PRIVATE docx2tiptap)

enable_testing()
add_test(NAME arena COMMAND test_arena)
add_test(NAME zip COMMAND test_zip)
add_test(NAME parse_minimal COMMAND test_parse WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
