## Directories

| *Directory*    | *Description*                                                     |
|:---------------|-------------------------------------------------------------------|
| `assets`       | Game assets                                                       |
| `dependencies` | Certain external dependencies that can be bundled with repository |
| `shaders`      | Rendering shaders                                            |
| `source`       | Source code                                                       |
| `tools`        | Various scripts (setup, 3D model format converters, etc...)       |

## Source directory

| *Source Sub-directory* | *Description*                                                                                                    | *Binary Output*                 |
|:-----------------------|------------------------------------------------------------------------------------------------------------------|---------------------------------|
| `client`               | Source code for the client application. Invokes code from all other modules except for `server`                      | Executable (`vkPhysics_client`) |
| `common`               | Headers / code that is common to all modules. Defines things like custom containers, utilities...                                                                     | Static library (`common`)       |
| `engine`               | Source code for *just* the game logic and things which are core to the engine (events, etc...)                   | Static library (`engine`)      |
| `net`               | Source code for the networking code - not only multiplayer but also communication with the meta server. Provides a sort of API that the client code will interface with in `cl_net_*` files | Static library (`net`)      |
| `renderer`             | Source code for module which handles rendering / interfacing with user (rendering, audio, input, window, etc...) | Static library (`renderer`)     |
| `server`               | Source code for the server console application. | Executable (`vkPhysics_server`) |
| `uiux`             | Source code for module which handles user interface and user experience (menus, scenes, etc...) | Static library (`renderer`)     |
