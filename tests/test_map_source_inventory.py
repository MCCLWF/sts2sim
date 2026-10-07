import importlib.util
from pathlib import Path
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "coverage" / "map_source_inventory.py"
SPEC = importlib.util.spec_from_file_location("map_source_inventory", SCRIPT)
inventory = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(inventory)


class SharedCppHeaderTests(unittest.TestCase):
    def test_resolves_shared_definition_but_ignores_forward_declaration(self):
        with tempfile.TemporaryDirectory() as directory:
            models_dir = Path(directory)
            (models_dir / "ForwardOnly.h").write_text(
                "struct SharedModel;\nstruct Unrelated { };\n", encoding="utf-8"
            )
            implementation = models_dir / "Shared.h"
            implementation.write_text(
                "struct SharedModel : Base {\n  void Act();\n};\n", encoding="utf-8"
            )

            self.assertEqual(inventory.find_shared_cpp_header(models_dir, "SharedModel"), implementation)

    def test_forward_declaration_alone_is_not_a_definition(self):
        with tempfile.TemporaryDirectory() as directory:
            models_dir = Path(directory)
            (models_dir / "ForwardOnly.h").write_text("struct SharedModel;\n", encoding="utf-8")

            self.assertIsNone(inventory.find_shared_cpp_header(models_dir, "SharedModel"))


if __name__ == "__main__":
    unittest.main()
