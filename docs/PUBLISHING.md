# Publishing this package on GitHub and Zenodo

GitHub gives the package a public URL. Zenodo gives it a permanent DOI. Do both once,
before the paper goes to review, so that the paper can cite them.

Use **one** of the two Zenodo routes below, not both. Each route mints its own DOI.

## 1. GitHub

### 1.1 Create the repository (github.com > + > New repository)

| Field | Value |
|---|---|
| Owner | your account |
| Repository name | `3gpp-256nca-6g-eval` |
| Description | Full-mode implementation and evaluation of the 3GPP 256-NCA4/5/6 algorithms (Snow 5G, AES-256, ZUC-256) for 6G on ESP32, Raspberry Pi 5, AMD Zen 4 and Spartan-7 FPGA: code, raw data and scripts. |
| Visibility | Public |
| Add a README file | off (the package has one) |
| Add .gitignore | No .gitignore (the package has one) |
| Choose a license | None (the package has LICENSE and LICENSE-DATA.md) |

### 1.2 Upload the files

The package has about 500 files. The browser upload accepts at most 100 files per commit,
so use GitHub Desktop or the command line.

GitHub Desktop (https://desktop.github.com):
1. File > Clone repository > choose `3gpp-256nca-6g-eval` > Clone.
2. Copy everything from inside the unzipped package folder into the cloned folder,
   including the hidden files `.gitignore` and `.zenodo.json`.
3. In GitHub Desktop, write the summary `Initial release: code, data and scripts`,
   click "Commit to main", then "Push origin".

Command line:
```
cd 3gpp-256nca-6g-eval
git init -b main
git add .
git commit -m "Initial release: code, data and scripts"
git remote add origin https://github.com/SohelRana92/3gpp-256nca-6g-eval.git
git push -u origin main
```

Check before pushing: `find . -name 'annex*.cpp'` must print nothing.

### 1.3 The "About" box (gear icon next to "About" on the repository page)

| Field | Value |
|---|---|
| Description | same as 1.1 |
| Website | the Zenodo DOI link, once it exists (`https://doi.org/10.5281/zenodo.<number>`) |
| Topics | `6g`, `5g`, `3gpp`, `cryptography`, `authenticated-encryption`, `stream-cipher`, `aes-256`, `zuc`, `snow`, `benchmarking`, `fpga`, `vhdl`, `esp32`, `raspberry-pi`, `avx512`, `side-channel` |
| Include in the home page | Releases on; Packages, Deployments off |

## 2. Zenodo, route A (recommended): automatic from a GitHub release

1. Sign in at https://zenodo.org with "Log in with GitHub".
2. Account menu > GitHub > find `3gpp-256nca-6g-eval` > switch it On.
3. On GitHub: Releases > Draft a new release.
   - Choose a tag: `v1.0.0` (create new tag on publish)
   - Release title: `v1.0.0: code and data for the submitted paper`
   - Description: `Code, raw data and scripts for the paper submitted to Cyber Security and Applications.`
   - Set as the latest release: on. Publish release.
4. Zenodo archives the release within a few minutes and mints a DOI. It takes the title,
   authors, licence and keywords from `.zenodo.json`.

## 3. Zenodo, route B: manual upload

zenodo.org > New upload. Upload the single file `3gpp-256nca-6g-eval_v1.0.0.zip`, then fill:

| Field | Value |
|---|---|
| Do you already have a DOI? | No |
| Resource type | Software |
| Title | Full-mode implementation and evaluation of the 3GPP 256-NCA4, 256-NCA5 and 256-NCA6 algorithms for 6G: code and data |
| Publication date | the upload date |
| Creators | Rana, Sohel (Person; affiliation: Bangladesh University of Engineering and Technology; role: Contact person) and Mondal, M. Rubaiyat Hossain (Person; affiliation: Bangladesh University of Engineering and Technology) |
| Description | copy from `.zenodo.json` |
| Licenses | MIT License (add Creative Commons Attribution 4.0 International for the data) |
| Keywords and subjects | 6G security; 3GPP; 256-bit algorithms; Snow 5G; AES-256; ZUC-256; authenticated encryption; FPGA; benchmarking; timing side channel |
| Version | 1.0.0 |
| Language | English |
| Related works | "Is supplement to" > URL > `https://github.com/SohelRana92/3gpp-256nca-6g-eval` (Resource type: Software) |
| Funding | none |
| Visibility | Public |

Then Publish. Zenodo shows the DOI at the top right of the record.

## 4. After the DOI exists

- `README.md`: replace "DOI to be added after the first Zenodo release" with the DOI.
- `CITATION.cff`: add `doi: 10.5281/zenodo.<number>` and `repository-code: https://github.com/SohelRana92/3gpp-256nca-6g-eval`.
- Supplement (`paper/supplementary/supplementary.tex`): fill `\repourl` and `\zenodo`.
- Main paper, Data availability: the GitHub URL and the DOI.
- Commit these changes. With route A, a new release `v1.0.1` gets a new version DOI under the
  same concept DOI; cite the concept DOI in the paper so that it always points to the latest
  version.

## 5. Later versions

Change the files, commit, and publish a new release (`v1.1.0`, ...). Earlier versions stay
available unchanged.
