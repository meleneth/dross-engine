# Third-party notices

Dross Engine is licensed separately under the MIT License in `LICENSE`.
Applications and games built with Dross Engine are not required to adopt the
Dross Engine license. Distributions containing Dross Engine or its dependencies
must retain the notices required by those components.

## Godot Engine

The packaged Linux prototype includes Godot Engine 4.7.1, distributed under the
MIT License:

Copyright (c) 2014-present Godot Engine contributors.

Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES, OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT, OR OTHERWISE, ARISING FROM,
OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Godot contains compatible third-party components with their own notices. The
authoritative notices are exposed by Godot through `Engine.get_license_info()`
and `Engine.get_copyright_info()` and maintained in Godot's `COPYRIGHT.txt`:
https://github.com/godotengine/godot/blob/4.7.1-stable/COPYRIGHT.txt

## Dross Engine native dependencies

Exact pins and upstream repositories are recorded in
`docs/dependency-lock.md`.

| Component | License |
| --- | --- |
| godot-cpp | MIT |
| EnTT | MIT |
| Boost.Ext.SML | BSL-1.0 |
| eventpp | Apache-2.0 |
| pcg-cpp | Apache-2.0 or MIT |
| tl::expected | CC0-1.0 |
| nlohmann/json | MIT |
| BLAKE3 | CC0-1.0 or Apache-2.0 |
| fmt | MIT |
| spdlog | MIT |

Build and test dependencies not shipped in the prototype are inventoried in
`docs/dependency-lock.md`, including CPM.cmake, Catch2, RapidCheck, and GdUnit4.
