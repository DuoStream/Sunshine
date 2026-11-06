# Duo microphone

This branch applies the microphone prototype attached to [DuoStream/Duo#549](https://github.com/DuoStream/Duo/issues/549).

The original files are `duo-sunshine-microphone.patch` on this repository and `duo-rdp-microphone.patch` on [DuoStream/aardwolf](https://github.com/DuoStream/aardwolf). The matching aardwolf changes are on [RedCuckoo/aardwolf `duomic-microphone`](https://github.com/RedCuckoo/aardwolf/tree/duomic-microphone).

The client must be [moonlight-mic v0.1.0](https://github.com/JimothySnicket/moonlight-mic/releases). In that client, enable **Stream client microphone to host**. Stock Moonlight does not send microphone audio.

Sunshine decodes the microphone on the existing encrypted control stream and writes it into the Duo session. The aardwolf `DuoRdp` helper then exposes it inside that session as the **Remote Audio** recording device. No extra microphone driver or network port is required.

The audio Sunshine sends back to the client has that same microphone removed, so an open mic is not played back to the person speaking. Push-to-talk never put the speaker's own voice into that mix. An open mic did, because the game played it into **Remote Audio** and Sunshine forwarded the whole mix.

## Build

Build this branch with the normal Windows steps in [building.md](building.md). The Windows build needs the `enet` submodule, Node.js on `PATH` for the web UI, and the static Qt dependencies (`libtiff`, HarfBuzz, FreeType).

Build `DuoRdp.exe` from the aardwolf branch above. `librlers` is a Rust extension, so the build needs a Rust compiler and the Visual Studio C++ linker (`link.exe`). The Python environment also needs the packages `aardwolf` depends on (`asn1tools`, `cryptography`, and the `unicrypto` backends). `builder\pyinstaller\build.bat` only succeeds when its working directory stays on the same drive as the repository. The hidden imports in that script have to include `unicrypto.backends.cryptography` and `unicrypto.backends.pycryptodomex`, or the packed executable fails at startup.

## Install into Duo

Stop the Duo service first. Keep the previous binaries.

- Replace `C:\Program Files\Duo\sunshine.exe`.
- Replace `C:\Program Files\Duo\DuoRdp.exe`.
- Copy this build's `build\assets\web` over `C:\Program Files\Duo\assets\web`. Duo's old pages are separate HTML files. This Sunshine build returns one page for every address, so the old files make every tab show the main page while the address stays on the tab. Keep Duo's `assets\web\duo` folder if you still want those icons.
- Leave **Stream Audio** enabled. That is the game audio sent to Moonlight, and it is the stream the microphone removal runs on.

Put `DuoMic-Rdp.json` next to `DuoRdp.exe`. The profile key is the Windows username of the Duo instance, in lowercase. `instanceName` is the instance name.

```json
{
  "schemaVersion": 1,
  "profiles": {
    "theuser": {
      "enabled": true,
      "source": "moonlight",
      "instanceName": "Instance"
    }
  }
}
```

Start the Duo service, start the instance, then connect with moonlight-mic. Inside the session, the microphone is **Remote Audio**.
