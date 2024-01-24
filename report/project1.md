## Project 1 Report


### 1. Basic information
 - Team #: 22
 - Github Repo Link: 
 - Student 1 UCI NetID:sjdesai
 - Student 1 Name:Sahil Desai
 - Student 2 UCI NetID (if applicable):
 - Student 2 Name (if applicable):


### 2. Internal Record Format
- Show your record format design.
    My records were formatted by [null-indicators][value1][value2]....
for all values that aren't null. Using the recordDescription to know which field is being accessed.


- Describe how you store a null field.
The null values aren't stored in the record, the null-indicators are used to know which fields are represented as null


- Describe how you store a VarChar field.
The varchar gets stored as follows: [sizeof varchar][value of varchar]. The size is represented as an int and stores how many bytes the varchar is.


- Describe how your record design satisfies O(1) field access.
I don't think that it satisfies O(1) since its not linear time over all record sizes. Getting a field requires adding the offsets of n fields before it. 
This is something that needs to be corrected moving forward


### 3. Page Format
- Show your page format design.
Each page was designed to store the records at the begginning followed by the Slot Directory. I created 2 structs, one for the Slots(offset,length) and one for PageInfo(freeSpacePointer, numSlots). These were stored at the end of each page for organization and access.


- Explain your slot directory design if applicable.
As explained above it was split into 2 structs, one containing information about the records and one about the page.


### 4. Page Management
- Show your algorithm of finding next available-space page when inserting a record.
availableSpace = PAGE_SIZE - (pageInfo->freeSpaceOffset + pageInfo->numSlots*sizeof(Slot) + sizeof(Slot) + sizeof(PageInfo)).
 This takes into account the slot directory info and adding a new slot for the additional record


- How many hidden pages are utilized in your design?
one per file


- Show your hidden page(s) format design if applicable
an array containing the counters


### 5. Implementation Detail
- Other implementation details goes here.



### 6. Member contribution (for team of two)
- Explain how you distribute the workload in team.



### 7. Other (optional)
- Freely use this section to tell us about things that are related to the project 1, but not related to the other sections (


- Feedback on the project to help improve the project. (optional)