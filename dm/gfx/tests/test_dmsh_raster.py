import ast
from pathlib import Path
import re
import unittest

TOOLS = Path(__file__).resolve().parents[1] / 'tools'


def builder_raster():
    tree = ast.parse((TOOLS / 'dmsh_build.py').read_text())
    names = {'raster_facts', 'const_scalar'}
    selected = [n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name in names]
    ns = {'re': re}
    exec(compile(ast.Module(body=selected, type_ignores=[]), str(TOOLS / 'dmsh_build.py'), 'exec'), ns)
    return ns['raster_facts']


RASTER = builder_raster()

TEMPLATE = '''struct main0_out { float4 gl_Position [[position]]; float2 m_5 [[user(locn0)]]; };
struct main0_in { float3 in_attr0 [[attribute(0)]]; };
vertex main0_out main0(main0_in in [[stage_in]])
{
    main0_out out = {};
%s
    return out;
}
'''

PASS_THROUGH = '''    float _41 = in.in_attr0.x;
    float _42 = in.in_attr0.y;
    float _43 = in.in_attr0.z;
    out.gl_Position.x = _41;
    out.gl_Position.y = _42;
    out.gl_Position.z = _43;
    out.gl_Position.w = 1.0;'''

CONSTANT_Z = '''    float _41 = in.in_attr0.x;
    float _42 = in.in_attr0.y;
    out.gl_Position.x = _41;
    out.gl_Position.y = _42;
    out.gl_Position.z = 0.0;
    out.gl_Position.w = 1.0;'''


def vertex(body):
    return RASTER('vs', TEMPLATE % body)


class PositionFacts(unittest.TestCase):
    def test_position_taken_as_is_keeps_its_attribute(self):
        got = vertex(PASS_THROUGH)
        self.assertEqual(got['pos_attr'], 0)
        self.assertEqual(got['pos_w'], 'one')
        self.assertNotIn('cov_attr', got)

    def test_constant_z_is_for_the_load_elision_alone(self):
        got = vertex(CONSTANT_Z)
        self.assertEqual(got, {'pos_attr': -1, 'cov_attr': 0, 'cov_z': 0.0, 'cov_w': 'one'})

    def test_constant_z_through_a_variable(self):
        body = CONSTANT_Z.replace('out.gl_Position.z = 0.0;', 'float _44 = 0.5;\n    out.gl_Position.z = _44;')
        self.assertEqual(vertex(body)['cov_z'], 0.5)

    def test_variable_set_twice_is_not_constant(self):
        body = CONSTANT_Z.replace('out.gl_Position.z = 0.0;', 'float _44 = 0.5;\n    _44 = _41;\n    out.gl_Position.z = _44;')
        self.assertEqual(vertex(body), {'pos_attr': -1, 'pos_why': 4})

    def test_z_outside_the_unit_range_is_refused(self):
        for z in ('2.0', '-0.5'):
            self.assertEqual(vertex(CONSTANT_Z.replace('= 0.0;', f'= {z};')), {'pos_attr': -1, 'pos_why': 4})

    def test_flipped_y_is_refused(self):
        self.assertEqual(vertex(CONSTANT_Z.replace('out.gl_Position.y = _42;', 'out.gl_Position.y = -_42;')), {'pos_attr': -1, 'pos_why': 3})

    def test_w_from_another_source_is_refused(self):
        self.assertEqual(vertex(CONSTANT_Z.replace('w = 1.0;', 'w = _41;')), {'pos_attr': -1, 'pos_why': 5})

    def test_control_flow_is_refused(self):
        self.assertEqual(vertex(CONSTANT_Z + '\n    if (_41 > 0.0) { out.gl_Position.z = 1.0; }'), {'pos_attr': -1, 'pos_why': 1})

    def test_x_not_from_an_attribute_is_refused(self):
        self.assertEqual(vertex(CONSTANT_Z.replace('out.gl_Position.x = _41;', 'out.gl_Position.x = 0.0;')), {'pos_attr': -1, 'pos_why': 2})


if __name__ == '__main__':
    unittest.main()
