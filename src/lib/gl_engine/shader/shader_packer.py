#!/usr/bin/env python3

import os
import re
import sys
from typing import List


def file_to_hex(file_path: str) -> str:
    """
    convert a file content into hex string liek 0xaa,0xbb,...
    """
    file_path = file_path.strip()
    with open(file_path, 'rb') as f:
        content = f.read()
        hex_str = content.hex()
        return re.sub('([0-9a-f][0-9a-f])', '0x\\1,', hex_str)


def pack_shader_files(output_name: str, shader_list: List[str]):
    file_content = "#pragma once\n\n\n"

    file_content += "// this file is auto generated do not edit it directly \n"

    for shader in shader_list:
        shader_file_name = os.path.basename(shader)
        # convert file name to underscore linked string
        shader_file_name = re.sub(r"\.", r"_", shader_file_name)
        # shader content
        shader_content = file_to_hex(shader)
        # shader content declaration
        shader_content_dec = "const char {0} [] = {1} {2} {3};\n".format(
            shader_file_name, "{", shader_content, "}")
        shader_content_size = "const unsigned {0}_size = sizeof({1});\n".format(
            shader_file_name, shader_file_name)

        file_content += shader_content_dec
        file_content += shader_content_size

    out_file = open(output_name, "w")
    out_file.write(file_content)
    out_file.close()


if __name__ == '__main__':
    # target file name
    output_name = sys.argv[1]
    # shader file lists
    shader_list = sys.argv[2:]

    pack_shader_files(output_name, shader_list)
