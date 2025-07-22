# GSheet MADS Plugin

This plugin connects a Google Sheet to MADS, allowing automated data ingestion from a spreadsheet (usually from Google Forms responses) and streaming to the MADS message broker. It supports periodic polling, customizable timestamps, and persistence between runs.

## Features

* Reads data from a **Google Sheet** via a Python script.
* Publishes messages to a configurable MADS topic.
* Prevents duplicates using a timestamp field (default: Horodateur).
* Configurable polling interval and timestamp parsing format.
* Saves the last timestamp to disk for memory persistence between restarts.

## Requirements

* MADS platform installed and configured.
* A Google Sheet (e.g., from Google Forms).
* A Google Cloud service account with a valid credentials.json.

## Installation

1. Copy the plugin

Place the plugin folder inside a gsheet directory.

2. Build and install

On your terminal, go inside your gsheet directory and type :

```bash
cmake -Bbuild -DCMAKE_INSTALL_PREFIX="$(mads -p)"
cmake --build build -j4
sudo cmake --install build
```

## MADS Configuration in the INI settings

The plugin supports the following settings in the INI file :

```ini
[gsheet]
pub_topic = "gsheet"
plugin = "gsheet.plugin"
script_path = "path/to/fetch_gsheet.py"
agent_id = "gsheet-agent"
poll_interval_minutes = 2
timestamp_field = "Horodateur"
timestamp_format = "%d/%m/%Y %H:%M:%S"
```

**pub_topic** : Topic on which messages will be published.

**plugin** : Name of the compiled plugin.

**script_path** : Path to your Python script that fetches Google Sheet data.

**agent_id** : Optional ID added to the message for identification.

**poll_interval_minutes** : Interval between polls.

**timestamp_field** : Name of the field containing the timestamp (default: Horodateur).

**timestamp_format** : Format of the timestamp (C++ style, default: French format).

## Google Sheets Setup

If not done yet for your Google Sheets, follow the next steps :

1. Create a Service Account

In [Google Cloud Console](https://console.cloud.google.com):
* Enable Google Sheets API and Google Drive API.
* Create a service account and download the credentials.json file.

2. Share Your Sheet

* Open your Google Sheet.
* Share it with the email address listed in client_email from credentials.json.
* Paste the credentials.json key in the /src directory next to the python file.

## Python Script

Once installed you must change the python script with your own Google Sheet settings :

```bash
# Replace with your actual path to the json key
gc = gspread.service_account(filename="path/to/credentials.json")

# Replace with your actual spreadsheet and worksheet names
spreadsheet = gc.open("My Spreadsheet Name")
worksheet = spreadsheet.worksheet("My Worksheet Tab")
```

This script should output all spreadsheet rows in JSON format. Point to this script in your mads.ini.

## Persistent Timestamp File

The plugin stores the last seen timestamp in a file named last_timestamp.txt under a hidden directory in the main directory. This ensures rows are not reprocessed after restarting the plugin.

## Notes

Your sheet must include a timestamp field (e.g., Horodateur) that matches the expected format entered in the mads.ini file.

MongoDB indexing (if used) should consider the message.timestamp_field to enforce uniqueness.

Timestamps must be in chronological order to ensure correct filtering.

## Authors

Main author: 
Léandre Reynard (Université Savoie Mont-Blanc)

Contributors:
Guillaume Cohen (Université de Toulouse) &
Anna-Carla Araujo (INSA Toulouse)

