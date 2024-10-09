# vim: set sts=2 ts=8 sw=2 tw=99 et:
import sys
from ambuild2 import run

# Simple extensions do not need to modify this file.

builder = run.BuildParser(sourcePath = sys.path[0], api='2.2')

builder.options.add_argument('--hl2sdk-root', type=str, dest='hl2sdk_root', default=None,
		                   help='Root search folder for HL2SDKs')
builder.options.add_argument('--mms-path', type=str, dest='mms_path', default=None,
                       help='Path to Metamod:Source')
builder.options.add_argument('--sm-path', type=str, dest='sm_path', default=None,
                       help='Path to SourceMod')
builder.options.add_argument('--enable-debug', action='store_const', const='1', dest='debug',
                       help='Enable debugging symbols')
builder.options.add_argument('--enable-optimize', action='store_const', const='1', dest='opt',
                       help='Enable optimization')
builder.options.add_argument('-s', '--sdks', default='all', dest='sdks',
                       help='Build against specified SDKs; valid args are "all", "present", or '
                            'comma-delimited list of engine names (default: %default)')
builder.options.add_argument('--targets', type=str, dest='targets', default=None,
                          help="Override the target architecture (use commas to separate multiple targets).")

builder.Configure()
