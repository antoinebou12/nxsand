# CI and releases

NXSand uses GitHub Actions for continuous build and manual release packaging.

## Workflows

| Workflow | Trigger | Output |
|----------|---------|--------|
| [`native-nro.yml`](https://github.com/antoinebou12/nxsand/blob/main/.github/workflows/native-nro.yml) | Push / PR to `main` | `make test`, Switch `make` + `make dist`, Linux and Windows desktop sanity builds |
| [`release.yml`](https://github.com/antoinebou12/nxsand/blob/main/.github/workflows/release.yml) | `workflow_dispatch` + tag | Portable zips via `scripts/package-release.py`, GitHub Release assets |
| [`docs.yml`](https://github.com/antoinebou12/nxsand/blob/main/.github/workflows/docs.yml) | Push to `main` (docs paths) | GitHub Pages site |

## CI artifact (Switch)

**NXSand-switch** zip contains `switch/NXSand.nro`. Unzip at the SD card root so the file lands at `sdmc:/switch/NXSand.nro`.

## Release zips

| Zip | Contents |
|-----|----------|
| `NXSand-switch-v*.zip` | `switch/NXSand.nro` |
| `NXSand-linux-v*.zip` | `NXSand`, `shaders/`, `romfs/fonts/`, `run.sh` |
| `NXSand-windows-v*.zip` | `NXSand.exe`, `NXSand-run.bat`, ANGLE/SDL DLLs, `shaders/`, fonts |

Local dry-run of the release workflow:

```powershell
powershell -File scripts/run-act-ci.ps1 -Workflow release -Tag v0.0.1
```

## Documentation deployment

The docs workflow builds with MkDocs Material and publishes to:

**https://antoinebou12.github.io/nxsand/**

Enable **Settings → Pages → Build and deployment → GitHub Actions** if the site does not appear after the first successful docs run.

![CI and release overview](diagrams/ci-release-flow.svg)
