## Project 2 Report


### 1. Basic information
 - Team #: 22
 - Github Repo Link: https://github.com/sahil60d/cs222-winter24-sahil60d.git
 - Student 1 UCI NetID: sjdesai
 - Student 1 Name: Sahil Desai
 - Student 2 UCI NetID (if applicable):
 - Student 2 Name (if applicable):

### 2. Meta-data
- Show your meta-data design (Tables and Columns table) and information about each column.
  - ## Tables
    - table_id, table_name, file_name

  - Columns
    - table_id, column_name, column_type, column_length, column_position


### 3. Internal Record Format (in case you have changed from P1, please re-enter here)
- Show your record format design.  
  - Records are organized by [numFields][nullIndicator][dataOffsets][data]
  - The records are organized on the page using a page info directory containing num slots and free space and slot directories with lengths and offsets of each record.


- Describe how you store a null field.
    - null fields aren't stored, they are found using the null indicator bytes. The field offsets in the record point to the next available field if null

- Describe how you store a VarChar field.
    - first 4 bytes(int) store the length of the varchar. The following bytes contain that many bytes representing the varchar


- Describe how your record design satisfies O(1) field access.
    - Including the offsets of the fields in each record allow me to go straight to that field after calculating the offset, rather than parsing through each field in the record. 


### 4. Page Format (in case you have changed from P1, please re-enter here)
- Show your page format design.
    - Records at the start growing down the page
    - Page Info struct at the end containing numSlots and freeSpace
    - Slot Directory starting right before the Page Info struct growing up the page. Contains length and offset values and flag for tombstone records


- Explain your slot directory design if applicable.
  - I created a struct that stores the length, offset, and bool flag for is the record is a tombstone. The slots directory grows up the page until out of space 


### 5. Page Management (in case you have changed from P1, please re-enter here)
- How many hidden pages are utilized in your design?
    - I have one hidden page to store the file metadata (read, write, append counts)


- Show your hidden page(s) format design if applicable
    - int array stored at the beginning of the page. 
    - [readpagecounter][writepagecounter][appendpagecounter]


### 6. Describe the following operation logic.
- Delete a record
    - use freeSpaceOffset to calc the amount of memory after the deleted record
    - memcpy that amount of data to the position of the deleted record
    - move freeSpaceOffset back the length of the deleted record
    - for all slots with an offset greater than the offset of the deleted record, decrement the offset by length of deleted record.
    - store -1 for length and offset and false for tombStone in the slot of the deleted record. Makes it available for future inserts.


- Update a record
  - check if size if <, =, or > than previous record.
  - If its the same size just replace the data
  - if its less, replace the data, compress the page and update the slot directories and the freeSpaceOffset
  - If its larger and can fit on the page, memcpy the following records back the amount of space needed to add the new record, then add the record and update the slots and page info
  - If its to large to fit on the page, use insert to add it to another page. Replace the data with RID of the updated record.


- Scan on normal records
    - iterates using GetNextRecord using a saved RID. This will scan the entire file by reading all the records before returning EOF. If a record with the required value is found, the requested attributes are copied into the data. 


- Scan on deleted records
    - If records don't exist EOF will be reached and return a faiure.
    - If the file doesn't exist, openFile will return a failure.


- Scan on updated records
    - Any time a tombstone is found, readRecord is recursively called until data is found. (Slot directory has isTomb flag)


### 7. Implementation Detail
- Other implementation details goes here.



### 8. Member contribution (for team of two)
- Explain how you distribute the workload in team.



### 9. Other (optional)
- Freely use this section to tell us about things that are related to the project 1, but not related to the other sections (optional)



- Feedback on the project to help improve the project. (optional)