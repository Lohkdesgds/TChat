# TChat (Twitch Chat) App!

This is an useful tool to watch chat!

## How to compile it:

- Get CMake working with GCC or another compiler
- Configure this as any other (this is a quick readme for now)
- Create an App on [Twitch Developers](https://dev.twitch.tv/console/apps)
  - Name it whatever
  - Category: `Chat Bot`
  - Type: `Confidential`
- Copy ID and Secret
- Create file under `resources/config.json`
- Put there and save:
    ```json
    {
        "client_id": "CLIENT_ID",
        "client_secret": "CLIENT_SECRET"
    }
    ```
- Build
- Run