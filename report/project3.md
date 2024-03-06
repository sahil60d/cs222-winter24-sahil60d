## Project 3 Report


### 1. Basic information
 - Team #: 22 
 - Github Repo Link: https://github.com/sahil60d/cs222-winter24-sahil60d.git
 - Student 1 UCI NetID: sjdesai
 - Student 1 Name: Sahil Desai
 - Student 2 UCI NetID (if applicable):
 - Student 2 Name (if applicable):


### 2. Meta-data page in an index file
- Show your meta-data page of an index design if you have any. 
- Each page was marked as a LEAF or NON_LEAF node. This was done using the following struct:
  - typedef struct { NodeType type; PageSize size; PageNum prev; PageNum next; } NodeDesc;
  - This was written at the end of every page to provide the type (LEAF/NON_LEAF), size, and if it was a leaf then also contained page numbers for sibling nodes in linked list



### 3. Index Entry Format
- Show your index entry design (structure). 

  - entries on internal nodes:
    - entries on to internal nodes are done by first check node needs to be split. If not, I add the key and update their less than/greater than pointers. If split is needed, create new node, copy half the keys and push first key of new node to higher level. I ran into an issue with possibly recursively spliting interal nodes up multiple levels.
  
  - entries on leaf nodes:
    - before entering to leaf node, I check if adding would cause a split. if it does I create a new split node and copy half of the keys from the original node. Then I update the pointers to insure the nodes are both in the right place. I then decide which node to place the new entry in by comparing it to all the keys in both the original and split node. If no split if required than I add the entry in the appropriate spot and update meta-data



### 4. Page Format
- Show your internal-page (non-leaf node) design.
  - internal nodes use the following struct:
    - typedef struct { PageNum left; PageNum right; unsigned keySize; void *key; } KeyDesc;
    - This stores the key, key size, and where the key points to in the tree. I store the key size to easily skip over the keys without having to calc the key size everytime. The left PageNum is the node thats less than the key and the right PageNum is greater than/equal to the key. 



- Show your leaf-page (leaf node) design.
  - leaf nodes use the following struct:
    - typedef struct { unsigned numRIDS; unsigned keySize, void *key; } DataDesc;
    - Each struct represents one key. After a struct, the RIDS that map to that key are written into memory. I keep track of them using the numRIDS value. They are accessable by use i * sizeof(RID) after the DataDesc struct. This method allows me to account for multiple RIDs with the same key and not have store the key multiple times.



### 5. Describe the following operation logic.
- Split
  - When splitting, I find or create an available page to become the new split node. I then copy the second half of the keys from the original node to the new one. I update the pointers so they point to each other in the linked list and the appropriate previous and next nodes. Then have to push up the first key of the new split node to an internal node (doesn't work)


- Rotation (if applicable)



- Merge/non-lazy deletion (if applicable)



- Duplicate key span in a page
  - My leaf node format ensures that keys are duplicated. It allows one key to store multiple RIDs. If a key already exists in an internal node it won't be added



- Duplicate key span multiple pages (if applicable)



### 6. Implementation Detail
- Have you added your own module or source file (.cc or .h)? 
  Clearly list the changes on files and CMakeLists.txt, if any.



- Other implementation details:



### 7. Member contribution (for team of two)
- Explain how you distribute the workload in team.



### 8. Other (optional)
- Freely use this section to tell us about things that are related to the project 3, but not related to the other sections (optional)
  - I ran into issues with my insertEntry which lead to multiple bugs I was not able to fix. My node doesn't properly handle pushing keys up to internal nodes, especially if it causes more splits higher up the tree. This was very time-consuming and made it difficult to test the rest of my code. I feel that I have a good understanding of how b+ trees should be structured and how the logic works, but had to many smaller errors that caused me to not adequately complete the project.   


- Feedback on the project to help improve the project. (optional)
