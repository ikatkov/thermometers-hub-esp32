import gzip
import subprocess
from datetime import datetime

fileName = "lib/ElegantOTA/src/elop.cpp"


revision = (
    subprocess.check_output(["git", "describe", "--tags", "--always"])
    .strip()
    .decode("utf-8")
)


def generate_PROGMEM_content():
    with open("lib/ElegantOTA/src/index.html", "r") as file:
        html_content = file.read()
        html_content = html_content.replace(
            "%build_timestamp%", datetime.now().strftime("%Y-%m-%d %H:%M")
        )
        html_content = html_content.replace("%git_revision%", revision)
        compressed_content = gzip.compress(html_content.encode('utf-8'))
        # Convert compressed content to a string of comma-separated byte values
        byte_string = ",".join(str(byte) for byte in compressed_content)
        return (
            "// This is an autogenerated file \n// DO NOT EDIT\n"
            + '#include "elop.h"\n'
            + "const uint8_t ELEGANT_HTML["
            + str(len(compressed_content))
            + "] PROGMEM = {\n"
            + byte_string
            + "\n};"
        )


def update_template_header(content):
    with open(fileName, "w") as file:
        file.write(content)


# Generate header content
header_content = generate_PROGMEM_content()

# Update template header file
update_template_header(header_content)

# Print a message indicating that the update was successful
print(fileName, " has been updated successfully.")
