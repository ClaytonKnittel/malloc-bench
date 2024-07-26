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
