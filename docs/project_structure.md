## Directories

| *Directory*    | *Description*                                                     |
|:---------------|-------------------------------------------------------------------|
| `assets`       | Game assets                                                       |
| `dependencies` | Certain external dependencies that can be bundled with repository |
| `shaders`      | Rendering GLSL shaders                                            |
| `source`       | Source code                                                       |
| `tools`        | Various scripts (setup, 3D model format converters, etc...)       |

## Source directory

| *Source Sub-directory* | *Description*                                                                                                                                                                                                 | *Binary Output*                                                                                                                                                                                                      |
|:-----------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `common`               | Headers / code that is common to all high-level modules                                                                                                                                                       | Static library (`common`)                                                                                                                                                                                            |
| `game`                 | Source code for *just* the game logic                                                                                                                                                                         | Two executables: one which runs as a console application (`vkPhysics_server`) and doesn't link with `renderer`, and one which runs as a windowed application (`vkPhysics_client`) which links with `renderer` module |
| `meta`                  | Source code for server which serves as a "meta" server - when servers or clients start, they register to the meta server - client connects to meta to know which game servers are online and can be connected to | Executable (`vkPhysics_meta`)                                                                                                                                                                                         |
| `renderer`             | Source code for module which handles rendering / interfacing with user (rendering, audio, input, window, etc...)                                                                                              | Static library (`renderer`)                                                                                                                                                                                          |



