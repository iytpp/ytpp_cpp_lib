function(ytpp_configure_object_target target_name)
    target_compile_features(${target_name} PRIVATE cxx_std_23)
    target_compile_definitions(${target_name} PRIVATE UNICODE _UNICODE _LIB)

    if(MSVC)
        target_compile_options(${target_name} PRIVATE
            /utf-8
            /W3
            /sdl
            /permissive-
            /Zc:__cplusplus
            "$<$<CONFIG:Debug>:/Od>"
            "$<$<CONFIG:Release>:/O2>"
            "$<$<CONFIG:Release>:/Oi>"
            "$<$<CONFIG:Release>:/Gy>"
            "$<$<CONFIG:Release>:/GL>"
            "$<$<CONFIG:Release>:/Zi>"
        )
    endif()
endfunction()

function(ytpp_configure_library_target target_name export_name output_name include_directory)
    set_target_properties(${target_name} PROPERTIES
        EXPORT_NAME "${export_name}"
        OUTPUT_NAME "${output_name}"
    )

    target_compile_features(${target_name} INTERFACE cxx_std_23)
    target_compile_definitions(${target_name} INTERFACE UNICODE _UNICODE)
    if(MSVC)
        target_compile_options(${target_name} INTERFACE /utf-8 /permissive- /Zc:__cplusplus)
    endif()
    target_include_directories(${target_name} PUBLIC
        "$<BUILD_INTERFACE:${include_directory}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )
endfunction()
