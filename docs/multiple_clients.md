## Running multiple clients on the same machine (for testing purposes)

To run a client, the game stores some information about your user's profile (information which will be used to communicate with the meta webserver about the user account - username, tag, etc...).

The user profile information of the local client is stored in the assets directory, under the file `.user_meta`. An unregistered user will have the `.user_meta` containing just the string `NO_USER` (without any spaces after or newlines - just this).

When running `vkPhysics_client`, the `.user_meta` file will be read by default. If you wish to be able to run the application on the account of another user, simply add another file like `.user_meta_test` (you will need to set the contents of the file to `NO_USER`) in the assets directory, and run with the command line arguments as follows `vkPhysics_client -u assets/.user_meta_test` (or with the path to the profile file with the project directory as root). This will now prompt you to sign up or login.

You will then be able to run both clients on the same machine (given of course that the second is registered under a different account) and play in the same server as different players.
