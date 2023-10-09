# performance benchmark

## performance of TCE

| MODEL| WORLD_SIZE | TP| PP | DP | save/s | load/s |
| :-----: | :----: | :----: |:----: |:----: |:----: |:----: |
| 7B1 | 8 | 4 | 2 | 1 | 6.9 | 7.5 |
| 7B1 | 16 | 8 | 2 | 1| 4.6 | 4.6 |
| 7B1 | 32 | 8 | 4 | 1 | 4.0 | 2.9 |
| 7B1 | 32 | 1 | 1 | 32 | 7.9 | 16.1 |
| 175B | 128 | 8 | 8 | 2 | 10.6 | 8.9 |

## compared to file storage

test on SenseCore file storage: 175B, world size=128, TP=PP=8, DP=2, save consts 4min30s.