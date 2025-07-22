import gspread
import json

gc = gspread.service_account(filename="/home/mads/Devel/gsheet/src/credentials.json")
sh = gc.open("Questionnaire CN - Réponses pour récupération de données")
worksheet = sh.worksheet("Réponses au formulaire 1")

rows = worksheet.get_all_records()
print(json.dumps(rows))
