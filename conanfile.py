from conan import ConanFile

class MainProject(ConanFile):
    python_requires = "conan_template/[~=5]@robotkernel/stable"
    python_requires_extend = "conan_template.RobotkernelConanFile"

    name = "bridge_cli"
    description = "robotkernel service bridge command line interface"
    exports_sources = ["*", "!.gitignore"]
    requires = "robotkernel/[~=6]@robotkernel/snapshot"

