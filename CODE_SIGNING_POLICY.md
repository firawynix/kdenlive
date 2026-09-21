# Code signing policy

## Scope

The Firawynix Kdenlive signing identity covers only the GPL-licensed fork at
<https://github.com/firawynix/kdenlive>. It doesn't cover unrelated Firawynix
applications or private components.

## Upstream relationship

This repository is a visible fork of
<https://github.com/KDE/kdenlive>. Firawynix releases preserve the upstream GPL
license, copyright notices, and source history. Fork-specific changes remain
available in the public release branch.

## Build origin

Signing is permitted only for release artifacts that correspond to a public
version tag and whose complete build and packaging scripts are present in this
repository. The signing request must identify the repository, commit, build
run, artifact, version, and SHA-256 digest.

## Dependencies

The Windows package may include unmodified open-source runtime libraries from
KDE Craft. Their licenses and source projects must remain identifiable. A
proprietary binary isn't eligible for this signing identity.

## Approval and incidents

The `firawynix` maintainer account reviews release changes and approves signing
requests. Signing stops immediately if the source, build pipeline, or signing
account may be compromised. Maintainers then publish a security advisory and
revoke affected credentials when required.

