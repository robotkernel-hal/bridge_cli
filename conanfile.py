from conan import ConanFile

class MainProject(ConanFile):
    python_requires = "conan_template/[~=5]@robotkernel/stable"
    python_requires_extend = "conan_template.RobotkernelConanFile"

    name = "bridge_cli"
    description = "robotkernel service bridge command line interface"
    exports_sources = ["*", "!.gitignore"]
    requires = "robotkernel/6.0.0-vec-rework@robotkernel/unstable"

    def source(self):
        self.run(f"sed 's/AC_INIT(.*/AC_INIT([bridge_cli], [{self.version}], [{self.author}])/' configure.ac.in > configure.ac")
