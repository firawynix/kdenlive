# External Integrations

## Media Framework

- **Service:** MLT 7.
- **Purpose:** Timeline playback, filters, transitions, timewarp producers, and rendering.
- **Location:** `src/mltcontroller/`, `src/timeline2/`, and renderer targets.

## Online Resources

- **Service:** Configured media providers.
- **Purpose:** Search and download online media.
- **Location:** `src/onlineresources/`.
- **Pattern:** `QNetworkAccessManager`, `QNetworkRequest`, async reply signals, optional OAuth2.

## OpenTimelineIO

- **Purpose:** Timeline interchange.
- **Location:** `src/otio/`.

## Planned OpenRouter

- **Purpose:** Convert natural-language editing intent into a strict edit plan.
- **Authentication:** Bearer API key supplied outside project documents.
- **Boundary:** Only project metadata needed for planning is sent; source media stays local by default.

