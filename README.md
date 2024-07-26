## Implicit list malloc (no coalesce) results:

```
-------------------------------------------------------------------------
| trace                         | correct? | mega ops / s | utilization |
-------------------------------------------------------------------------
| traces/bdd-aa32.trace         |        Y |          0.2 |       58.3% |
| traces/bdd-aa4.trace          |        Y |          4.1 |       64.8% |
| traces/bdd-ma4.trace          |        Y |          0.4 |       58.0% |
| traces/bdd-nq7.trace          |        Y |          0.1 |       58.1% |
| traces/cbit-abs.trace         |        Y |          1.7 |       56.5% |
| traces/cbit-parity.trace      |        Y |          0.4 |       54.9% |
| traces/cbit-satadd.trace      |        Y |          0.4 |       57.6% |
| traces/cbit-xyz.trace         |        Y |          0.7 |       49.7% |
|*traces/ngram-fox1.trace       |        Y |        359.5 |       63.5% |
| traces/ngram-gulliver1.trace  |        Y |          1.2 |       31.2% |
| traces/ngram-gulliver2.trace  |        Y |          0.3 |       31.2% |
| traces/ngram-moby1.trace      |        Y |          0.6 |       29.5% |
| traces/ngram-shake1.trace     |        Y |          0.4 |       28.7% |
| traces/onoro.trace            |        Y |          0.0 |       56.6% |
| traces/onoro-cc.trace         |        Y |          8.0 |       75.0% |
| traces/server.trace           |        Y |          4.4 |       66.4% |
|*traces/simple.trace           |        Y |        300.3 |       57.4% |
|*traces/simple_calloc.trace    |        Y |        292.0 |       97.2% |
|*traces/simple_realloc.trace   |        Y |        281.4 |       96.2% |
| traces/syn-array.trace        |        Y |          0.1 |       78.4% |
|*traces/syn-array-short.trace  |        Y |        347.2 |       79.5% |
| traces/syn-mix.trace          |        Y |          0.2 |       77.8% |
| traces/syn-mix-realloc.trace  |        Y |         26.9 |       40.8% |
|*traces/syn-mix-short.trace    |        Y |        362.2 |       81.7% |
| traces/syn-string.trace       |        Y |          0.2 |       69.4% |
|*traces/syn-string-short.trace |        Y |        353.8 |       63.1% |
| traces/syn-struct.trace       |        Y |          0.2 |       70.0% |
|*traces/syn-struct-short.trace |        Y |        377.1 |       62.3% |
|*traces/test.trace             |        Y |        328.8 |       80.4% |
|*traces/test-zero.trace        |        Y |        236.0 |       76.0% |
-------------------------------------------------------------------------
* = ignored for scoring

Summary:
All correct? Y
Average utilization: 55.6%
Average mega ops / s: 0.5
```

## SlabMalloc results:

```
-------------------------------------------------------------------------
| trace                         | correct? | mega ops / s | utilization |
-------------------------------------------------------------------------
| traces/bdd-aa32.trace         |        Y |        264.1 |       82.3% |
| traces/bdd-aa4.trace          |        Y |        291.4 |       55.6% |
| traces/bdd-ma4.trace          |        Y |        287.8 |       80.7% |
| traces/bdd-nq7.trace          |        Y |        270.6 |       83.5% |
| traces/cbit-abs.trace         |        Y |        150.7 |       64.2% |
| traces/cbit-parity.trace      |        Y |        129.5 |       64.8% |
| traces/cbit-satadd.trace      |        Y |        132.6 |       72.2% |
| traces/cbit-xyz.trace         |        Y |        140.7 |       66.3% |
|*traces/ngram-fox1.trace       |        Y |        203.7 |       21.3% |
| traces/ngram-gulliver1.trace  |        Y |        150.1 |       59.4% |
| traces/ngram-gulliver2.trace  |        Y |        131.5 |       69.3% |
| traces/ngram-moby1.trace      |        Y |        139.3 |       62.3% |
| traces/ngram-shake1.trace     |        Y |        138.6 |       63.4% |
| traces/onoro.trace            |        Y |         52.3 |       83.8% |
| traces/onoro-cc.trace         |        Y |        138.2 |       74.9% |
| traces/server.trace           |        Y |        301.8 |       57.1% |
|*traces/simple.trace           |        Y |         93.5 |       19.2% |
|*traces/simple_calloc.trace    |        Y |        336.1 |       73.8% |
|*traces/simple_realloc.trace   |        Y |        103.6 |       82.0% |
| traces/syn-array.trace        |        Y |         50.1 |       80.8% |
|*traces/syn-array-short.trace  |        Y |        125.6 |       79.6% |
| traces/syn-mix.trace          |        Y |         77.1 |       80.2% |
| traces/syn-mix-realloc.trace  |        Y |        117.9 |       73.1% |
|*traces/syn-mix-short.trace    |        Y |        141.2 |       32.9% |
| traces/syn-string.trace       |        Y |        108.5 |       84.5% |
|*traces/syn-string-short.trace |        Y |        148.6 |       14.1% |
| traces/syn-struct.trace       |        Y |        105.1 |       87.0% |
|*traces/syn-struct-short.trace |        Y |        151.6 |       13.9% |
|*traces/test.trace             |        Y |        129.4 |       75.3% |
|*traces/test-zero.trace        |        Y |         83.9 |       48.0% |
-------------------------------------------------------------------------
* = ignored for scoring

Summary:
All correct? Y
Average utilization: 72.3%
Average mega ops / s: 140.8
```

## glibc malloc results:

```
-------------------------------------------------------------------------
| trace                         | correct? | mega ops / s | utilization |
-------------------------------------------------------------------------
| traces/bdd-aa32.trace         |        Y |        130.9 |         n/a |
| traces/bdd-aa4.trace          |        Y |        161.3 |         n/a |
| traces/bdd-ma4.trace          |        Y |        133.5 |         n/a |
| traces/bdd-nq7.trace          |        Y |        109.9 |         n/a |
| traces/cbit-abs.trace         |        Y |        132.1 |         n/a |
| traces/cbit-parity.trace      |        Y |        123.4 |         n/a |
| traces/cbit-satadd.trace      |        Y |        119.6 |         n/a |
| traces/cbit-xyz.trace         |        Y |        132.0 |         n/a |
|*traces/ngram-fox1.trace       |        Y |        280.0 |         n/a |
| traces/ngram-gulliver1.trace  |        Y |        157.5 |         n/a |
| traces/ngram-gulliver2.trace  |        Y |        155.9 |         n/a |
| traces/ngram-moby1.trace      |        Y |        151.6 |         n/a |
| traces/ngram-shake1.trace     |        Y |        149.4 |         n/a |
| traces/onoro.trace            |        Y |         13.4 |         n/a |
| traces/onoro-cc.trace         |        Y |        222.1 |         n/a |
| traces/server.trace           |        Y |        140.1 |         n/a |
|*traces/simple.trace           |        Y |        190.4 |         n/a |
|*traces/simple_calloc.trace    |        Y |        340.5 |         n/a |
|*traces/simple_realloc.trace   |        Y |        147.5 |         n/a |
| traces/syn-array.trace        |        Y |         29.7 |         n/a |
|*traces/syn-array-short.trace  |        Y |         86.9 |         n/a |
| traces/syn-mix.trace          |        Y |         50.9 |         n/a |
| traces/syn-mix-realloc.trace  |        Y |         53.9 |         n/a |
|*traces/syn-mix-short.trace    |        Y |        252.6 |         n/a |
| traces/syn-string.trace       |        Y |        100.2 |         n/a |
|*traces/syn-string-short.trace |        Y |        276.4 |         n/a |
| traces/syn-struct.trace       |        Y |        102.4 |         n/a |
|*traces/syn-struct-short.trace |        Y |        222.4 |         n/a |
|*traces/test.trace             |        Y |         84.4 |         n/a |
|*traces/test-zero.trace        |        Y |        140.9 |         n/a |
-------------------------------------------------------------------------
* = ignored for scoring

Summary:
All correct? Y
Average utilization: n/a
Average mega ops / s: 102.2
```

# TCMalloc results:

```
-------------------------------------------------------------------------
| trace                         | correct? | mega ops / s | utilization |
-------------------------------------------------------------------------
| traces/bdd-aa32.trace         |        Y |        240.6 |         n/a |
| traces/bdd-aa4.trace          |        Y |        390.5 |         n/a |
| traces/bdd-ma4.trace          |        Y |        308.2 |         n/a |
| traces/bdd-nq7.trace          |        Y |        183.6 |         n/a |
| traces/cbit-abs.trace         |        Y |        369.5 |         n/a |
| traces/cbit-parity.trace      |        Y |        349.2 |         n/a |
| traces/cbit-satadd.trace      |        Y |        354.1 |         n/a |
| traces/cbit-xyz.trace         |        Y |        358.8 |         n/a |
|*traces/ngram-fox1.trace       |        Y |        520.8 |         n/a |
| traces/ngram-gulliver1.trace  |        Y |        362.7 |         n/a |
| traces/ngram-gulliver2.trace  |        Y |        326.2 |         n/a |
| traces/ngram-moby1.trace      |        Y |        351.9 |         n/a |
| traces/ngram-shake1.trace     |        Y |        347.0 |         n/a |
| traces/onoro.trace            |        Y |         20.7 |         n/a |
| traces/onoro-cc.trace         |        Y |        317.3 |         n/a |
| traces/server.trace           |        Y |        374.7 |         n/a |
|*traces/simple.trace           |        Y |        438.6 |         n/a |
|*traces/simple_calloc.trace    |        Y |        479.1 |         n/a |
|*traces/simple_realloc.trace   |        Y |        398.6 |         n/a |
| traces/syn-array.trace        |        Y |        168.0 |         n/a |
|*traces/syn-array-short.trace  |        Y |        474.6 |         n/a |
| traces/syn-mix.trace          |        Y |        289.3 |         n/a |
| traces/syn-mix-realloc.trace  |        Y |        246.2 |         n/a |
|*traces/syn-mix-short.trace    |        Y |        504.5 |         n/a |
| traces/syn-string.trace       |        Y |        327.6 |         n/a |
|*traces/syn-string-short.trace |        Y |        227.7 |         n/a |
| traces/syn-struct.trace       |        Y |        334.4 |         n/a |
|*traces/syn-struct-short.trace |        Y |        506.8 |         n/a |
|*traces/test.trace             |        Y |        477.7 |         n/a |
|*traces/test-zero.trace        |        Y |        382.3 |         n/a |
-------------------------------------------------------------------------
* = ignored for scoring

Summary:
All correct? Y
Average utilization: n/a
Average mega ops / s: 269.4
```

## jemalloc results:

```
-------------------------------------------------------------------------
| trace                         | correct? | mega ops / s | utilization |
-------------------------------------------------------------------------
| traces/bdd-aa32.trace         |        Y |        244.5 |         n/a |
| traces/bdd-aa4.trace          |        Y |        305.3 |         n/a |
| traces/bdd-ma4.trace          |        Y |        247.9 |         n/a |
| traces/bdd-nq7.trace          |        Y |        226.8 |         n/a |
| traces/cbit-abs.trace         |        Y |        288.0 |         n/a |
| traces/cbit-parity.trace      |        Y |        251.6 |         n/a |
| traces/cbit-satadd.trace      |        Y |        267.4 |         n/a |
| traces/cbit-xyz.trace         |        Y |        283.3 |         n/a |
|*traces/ngram-fox1.trace       |        Y |        416.8 |         n/a |
| traces/ngram-gulliver1.trace  |        Y |        270.3 |         n/a |
| traces/ngram-gulliver2.trace  |        Y |        277.1 |         n/a |
| traces/ngram-moby1.trace      |        Y |        290.6 |         n/a |
| traces/ngram-shake1.trace     |        Y |        289.0 |         n/a |
| traces/onoro.trace            |        Y |         30.2 |         n/a |
| traces/onoro-cc.trace         |        Y |        302.1 |         n/a |
| traces/server.trace           |        Y |        263.3 |         n/a |
|*traces/simple.trace           |        Y |        350.2 |         n/a |
|*traces/simple_calloc.trace    |        Y |        376.2 |         n/a |
|*traces/simple_realloc.trace   |        Y |         40.5 |         n/a |
| traces/syn-array.trace        |        Y |         54.3 |         n/a |
|*traces/syn-array-short.trace  |        Y |         50.1 |         n/a |
| traces/syn-mix.trace          |        Y |        107.5 |         n/a |
| traces/syn-mix-realloc.trace  |        Y |         42.5 |         n/a |
|*traces/syn-mix-short.trace    |        Y |        367.4 |         n/a |
| traces/syn-string.trace       |        Y |        248.0 |         n/a |
|*traces/syn-string-short.trace |        Y |        429.4 |         n/a |
| traces/syn-struct.trace       |        Y |        250.9 |         n/a |
|*traces/syn-struct-short.trace |        Y |        427.6 |         n/a |
|*traces/test.trace             |        Y |         51.8 |         n/a |
|*traces/test-zero.trace        |        Y |        245.8 |         n/a |
-------------------------------------------------------------------------
* = ignored for scoring

Summary:
All correct? Y
Average utilization: n/a
Average mega ops / s: 193.4
```

## Alloc end and no free results:

This allocator always returns a block at the end of the heap, then extends the
heap to include that block. Free is a no-op, so memory is never reused. This is
a theoretical upper bound on throughput, since it does almost nothing.

```
-------------------------------------------------------------------------
| trace                         | correct? | mega ops / s | utilization |
-------------------------------------------------------------------------
| traces/bdd-aa32.trace         |        Y |        769.0 |       49.3% |
| traces/bdd-aa4.trace          |        Y |        999.7 |       47.6% |
| traces/bdd-ma4.trace          |        Y |        798.6 |       53.5% |
| traces/bdd-nq7.trace          |        Y |        687.3 |       67.5% |
| traces/cbit-abs.trace         |        Y |        629.9 |       23.2% |
| traces/cbit-parity.trace      |        Y |        539.6 |       20.2% |
| traces/cbit-satadd.trace      |        Y |        544.0 |       23.8% |
| traces/cbit-xyz.trace         |        Y |        576.5 |       20.8% |
|*traces/ngram-fox1.trace       |        Y |        946.9 |       93.3% |
| traces/ngram-gulliver1.trace  |        Y |        601.7 |       16.6% |
| traces/ngram-gulliver2.trace  |        Y |        514.8 |       16.2% |
| traces/ngram-moby1.trace      |        Y |        555.2 |       15.6% |
| traces/ngram-shake1.trace     |        Y |        541.1 |       15.1% |
| traces/onoro.trace            |        Y |         95.3 |       48.8% |
| traces/onoro-cc.trace         |        Y |        426.0 |       12.4% |
| traces/server.trace           |        Y |        835.6 |       16.6% |
|*traces/simple.trace           |        Y |        844.7 |       99.0% |
|*traces/simple_calloc.trace    |        Y |       1446.1 |       66.6% |
|*traces/simple_realloc.trace   |        Y |        664.8 |       99.8% |
| traces/syn-array.trace        |        Y |        548.4 |       25.9% |
|*traces/syn-array-short.trace  |        Y |       1008.5 |       77.8% |
| traces/syn-mix.trace          |        Y |        551.4 |       26.1% |
| traces/syn-mix-realloc.trace  |        Y |         20.4 |       19.4% |
|*traces/syn-mix-short.trace    |        Y |       1010.3 |       96.9% |
| traces/syn-string.trace       |        Y |        564.1 |       23.6% |
|*traces/syn-string-short.trace |        Y |       1015.9 |       92.5% |
| traces/syn-struct.trace       |        Y |        559.7 |       24.3% |
|*traces/syn-struct-short.trace |        Y |       1015.1 |       93.6% |
|*traces/test.trace             |        Y |       1028.2 |       78.0% |
|*traces/test-zero.trace        |        Y |       1324.9 |       99.9% |
-------------------------------------------------------------------------
* = ignored for scoring

Summary:
All correct? Y
Average utilization: 28.3%
Average mega ops / s: 470.0
```
