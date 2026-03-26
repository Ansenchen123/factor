#ifndef DATA_H
#define DATA_H

#include <stdbool.h>

#define MAX_NAME_LEN 64
#define MAX_ITEMS 32
#define MAX_EQUIPMENT 32
#define MAX_LINES 16

// Maintenance Item (e.g., Ink, Paper)
typedef struct {
    char name[MAX_NAME_LEN];
    int period_days;        // Maintenance cycle in days
    char last_checked_date[11]; // YYYY-MM-DD
    bool is_checked_today;  // Status for the current day
} MaintenanceItem;

// Equipment (e.g., Printer A)
typedef struct {
    char name[MAX_NAME_LEN];
    MaintenanceItem items[MAX_ITEMS];
    int item_count;
    // UI State for scrolling items
    int scroll_offset; 
} Equipment;

// Production Line (e.g., Line 1)
typedef struct {
    char name[MAX_NAME_LEN];
    Equipment equipments[MAX_EQUIPMENT];
    int equipment_count;
} ProductionLine;

// Application Data context
typedef struct {
    ProductionLine lines[MAX_LINES];
    int line_count;
    char current_date[11]; // System or stored date
    
    // Simulation Mode
    bool is_simulating;
    int sim_year;
    int sim_month;
    int sim_day;
} AppData;

// Global Data Instance
extern AppData g_appData;

// Functions
void InitData();
void LoadData();
void SaveData();
void AddLine(const char* name);
void AddEquipment(int lineIndex, const char* name);
void AddMaintenanceItem(int lineIndex, int equipmentIndex, const char* name, int period);
void CheckMaintenanceStatus(); 
// Maintenance Actions
void SetMaintenanceItemStatus(int lineIndex, int eqIndex, int itemIndex, bool status);

// Rename Functions
void UpdateLineName(int lineIndex, const char* newName);
void UpdateEquipmentName(int lineIndex, int eqIndex, const char* newName);
void UpdateItemName(int lineIndex, int eqIndex, int itemIndex, const char* newName);

// Management Functions
void DuplicateLine(int lineIndex);
void DeleteLine(int lineIndex); // Useful to have
void DeleteEquipment(int lineIndex, int eqIndex); // Useful
void DeleteItem(int lineIndex, int eqIndex, int itemIndex); // Useful

// Simulation
void GetCurrentDateStr(char* buffer);
int DaysBetween(const char* date1, const char* date2);
void SetSimulationDate(int year, int month, int day);
void ToggleSimulation(bool enable);

#endif // DATA_H
