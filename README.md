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
<<<<<<< HEAD
---------------------------------------------------------------------------
| trace                           | correct? | mega ops / s | utilization |
---------------------------------------------------------------------------
|*traces/bdd-aa32.trace           |        Y |        136.4 |        inf% |
|*traces/bdd-aa4.trace            |        Y |        166.2 |        inf% |
|*traces/bdd-ma4.trace            |        Y |        140.8 |        inf% |
|*traces/bdd-nq7.trace            |        Y |        109.2 |        inf% |
|*traces/cbit-abs.trace           |        Y |        125.0 |        inf% |
|*traces/cbit-parity.trace        |        Y |        119.8 |        inf% |
|*traces/cbit-satadd.trace        |        Y |        114.9 |        inf% |
|*traces/cbit-xyz.trace           |        Y |        125.1 |        inf% |
| traces/firefox.trace            |        Y |         69.7 |        inf% |
| traces/four-in-a-row.trace      |        Y |        117.9 |        inf% |
| traces/grep.trace               |        Y |        120.7 |        inf% |
| traces/haskell-web-server.trace |        Y |         82.2 |        inf% |
| traces/mc_server.trace          |        Y |         59.9 |        inf% |
| traces/mc_server_large.trace    |        Y |         69.5 |        inf% |
| traces/mc_server_small.trace    |        Y |         78.1 |        inf% |
|*traces/ngram-fox1.trace         |        Y |        253.6 |        inf% |
|*traces/ngram-gulliver1.trace    |        Y |        146.1 |        inf% |
|*traces/ngram-gulliver2.trace    |        Y |        153.8 |        inf% |
|*traces/ngram-moby1.trace        |        Y |        144.8 |        inf% |
|*traces/ngram-shake1.trace       |        Y |        143.4 |        inf% |
| traces/onoro.trace              |        Y |         15.9 |        inf% |
| traces/onoro-cc.trace           |        Y |        246.2 |        inf% |
| traces/py-catan-ai.trace        |        Y |         18.0 |        inf% |
| traces/py-euler-nayuki.trace    |        Y |         82.5 |        inf% |
| traces/scp.trace                |        Y |        118.3 |        inf% |
|*traces/server.trace             |        Y |        141.7 |        inf% |
|*traces/simple.trace             |        Y |        191.0 |        inf% |
|*traces/simple_calloc.trace      |        Y |        328.2 |        inf% |
|*traces/simple_realloc.trace     |        Y |        147.5 |        inf% |
| traces/solitaire.trace          |        Y |        123.0 |        inf% |
| traces/ssh.trace                |        Y |        118.0 |        inf% |
|*traces/syn-array.trace          |        Y |         26.7 |        inf% |
|*traces/syn-array-short.trace    |        Y |         78.2 |        inf% |
|*traces/syn-mix.trace            |        Y |         48.4 |        inf% |
|*traces/syn-mix-realloc.trace    |        Y |         47.6 |        inf% |
|*traces/syn-mix-short.trace      |        Y |        246.8 |        inf% |
|*traces/syn-string.trace         |        Y |         96.6 |        inf% |
|*traces/syn-string-short.trace   |        Y |        271.0 |        inf% |
|*traces/syn-struct.trace         |        Y |         99.5 |        inf% |
|*traces/syn-struct-short.trace   |        Y |        270.0 |        inf% |
|*traces/test.trace               |        Y |         95.0 |        inf% |
|*traces/test-zero.trace          |        Y |        161.0 |        inf% |
| traces/vim.trace                |        Y |        110.7 |        inf% |
| traces/vlc.trace                |        Y |        102.6 |        inf% |
---------------------------------------------------------------------------
* = ignored for scoring

Summary:
Average mega ops / s: 80.3
=======
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
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
```

# TCMalloc results:

```
<<<<<<< HEAD
---------------------------------------------------------------------------
| trace                           | correct? | mega ops / s | utilization |
---------------------------------------------------------------------------
|*traces/bdd-aa32.trace           |        Y |        120.6 |        inf% |
|*traces/bdd-aa4.trace            |        Y |        338.8 |        inf% |
|*traces/bdd-ma4.trace            |        Y |        290.8 |        inf% |
|*traces/bdd-nq7.trace            |        Y |        176.0 |        inf% |
|*traces/cbit-abs.trace           |        Y |        327.8 |        inf% |
|*traces/cbit-parity.trace        |        Y |        312.8 |        inf% |
|*traces/cbit-satadd.trace        |        Y |        319.6 |        inf% |
|*traces/cbit-xyz.trace           |        Y |        325.1 |        inf% |
| traces/firefox.trace            |        Y |        140.4 |        inf% |
| traces/four-in-a-row.trace      |        Y |        183.1 |        inf% |
| traces/grep.trace               |        Y |        316.6 |        inf% |
| traces/haskell-web-server.trace |        Y |        321.6 |        inf% |
| traces/mc_server.trace          |        Y |         89.5 |        inf% |
| traces/mc_server_large.trace    |        Y |         99.6 |        inf% |
| traces/mc_server_small.trace    |        Y |        194.8 |        inf% |
|*traces/ngram-fox1.trace         |        Y |        517.9 |        inf% |
|*traces/ngram-gulliver1.trace    |        Y |        329.2 |        inf% |
|*traces/ngram-gulliver2.trace    |        Y |        290.6 |        inf% |
|*traces/ngram-moby1.trace        |        Y |        314.9 |        inf% |
|*traces/ngram-shake1.trace       |        Y |        312.9 |        inf% |
| traces/onoro.trace              |        Y |         38.4 |        inf% |
| traces/onoro-cc.trace           |        Y |        294.4 |        inf% |
| traces/py-catan-ai.trace        |        Y |         20.9 |        inf% |
| traces/py-euler-nayuki.trace    |        Y |         76.2 |        inf% |
| traces/scp.trace                |        Y |        333.9 |        inf% |
|*traces/server.trace             |        Y |        367.7 |        inf% |
|*traces/simple.trace             |        Y |        477.5 |        inf% |
|*traces/simple_calloc.trace      |        Y |        507.5 |        inf% |
|*traces/simple_realloc.trace     |        Y |        414.3 |        inf% |
| traces/solitaire.trace          |        Y |        173.6 |        inf% |
| traces/ssh.trace                |        Y |        369.5 |        inf% |
|*traces/syn-array.trace          |        Y |        131.4 |        inf% |
|*traces/syn-array-short.trace    |        Y |        494.8 |        inf% |
|*traces/syn-mix.trace            |        Y |        253.2 |        inf% |
|*traces/syn-mix-realloc.trace    |        Y |        221.6 |        inf% |
|*traces/syn-mix-short.trace      |        Y |        533.1 |        inf% |
|*traces/syn-string.trace         |        Y |        285.8 |        inf% |
|*traces/syn-string-short.trace   |        Y |        542.9 |        inf% |
|*traces/syn-struct.trace         |        Y |        290.6 |        inf% |
|*traces/syn-struct-short.trace   |        Y |        515.8 |        inf% |
|*traces/test.trace               |        Y |        513.3 |        inf% |
|*traces/test-zero.trace          |        Y |        440.0 |        inf% |
| traces/vim.trace                |        Y |        236.7 |        inf% |
| traces/vlc.trace                |        Y |        144.9 |        inf% |
---------------------------------------------------------------------------
* = ignored for scoring

Summary:
Average mega ops / s: 149.2
=======
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
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
```

## jemalloc results:

```
<<<<<<< HEAD
---------------------------------------------------------------------------
| trace                           | correct? | mega ops / s | utilization |
---------------------------------------------------------------------------
|*traces/bdd-aa32.trace           |        Y |        189.7 |        inf% |
|*traces/bdd-aa4.trace            |        Y |        303.4 |        inf% |
|*traces/bdd-ma4.trace            |        Y |        245.3 |        inf% |
|*traces/bdd-nq7.trace            |        Y |        209.3 |        inf% |
|*traces/cbit-abs.trace           |        Y |        265.6 |        inf% |
|*traces/cbit-parity.trace        |        Y |        239.7 |        inf% |
|*traces/cbit-satadd.trace        |        Y |        243.9 |        inf% |
|*traces/cbit-xyz.trace           |        Y |        264.3 |        inf% |
| traces/firefox.trace            |        Y |        104.6 |        inf% |
| traces/four-in-a-row.trace      |        Y |        167.5 |        inf% |
| traces/grep.trace               |        Y |         88.9 |        inf% |
| traces/haskell-web-server.trace |        Y |         86.3 |        inf% |
| traces/mc_server.trace          |        Y |         61.7 |        inf% |
| traces/mc_server_large.trace    |        Y |         67.6 |        inf% |
| traces/mc_server_small.trace    |        Y |         80.9 |        inf% |
|*traces/ngram-fox1.trace         |        Y |        380.4 |        inf% |
|*traces/ngram-gulliver1.trace    |        Y |        251.5 |        inf% |
|*traces/ngram-gulliver2.trace    |        Y |        249.5 |        inf% |
|*traces/ngram-moby1.trace        |        Y |        267.1 |        inf% |
|*traces/ngram-shake1.trace       |        Y |        262.6 |        inf% |
| traces/onoro.trace              |        Y |         29.2 |        inf% |
| traces/onoro-cc.trace           |        Y |        267.1 |        inf% |
| traces/py-catan-ai.trace        |        Y |         41.4 |        inf% |
| traces/py-euler-nayuki.trace    |        Y |         59.9 |        inf% |
| traces/scp.trace                |        Y |         75.4 |        inf% |
|*traces/server.trace             |        Y |        257.7 |        inf% |
|*traces/simple.trace             |        Y |        336.1 |        inf% |
|*traces/simple_calloc.trace      |        Y |        403.0 |        inf% |
|*traces/simple_realloc.trace     |        Y |         34.8 |        inf% |
| traces/solitaire.trace          |        Y |        147.1 |        inf% |
| traces/ssh.trace                |        Y |        216.2 |        inf% |
|*traces/syn-array.trace          |        Y |         49.1 |        inf% |
|*traces/syn-array-short.trace    |        Y |         45.5 |        inf% |
|*traces/syn-mix.trace            |        Y |         98.6 |        inf% |
|*traces/syn-mix-realloc.trace    |        Y |         40.3 |        inf% |
|*traces/syn-mix-short.trace      |        Y |        365.6 |        inf% |
|*traces/syn-string.trace         |        Y |        220.3 |        inf% |
|*traces/syn-string-short.trace   |        Y |        423.4 |        inf% |
|*traces/syn-struct.trace         |        Y |        229.3 |        inf% |
|*traces/syn-struct-short.trace   |        Y |        421.6 |        inf% |
|*traces/test.trace               |        Y |         58.5 |        inf% |
|*traces/test-zero.trace          |        Y |        308.8 |        inf% |
| traces/vim.trace                |        Y |         71.1 |        inf% |
| traces/vlc.trace                |        Y |        146.1 |        inf% |
---------------------------------------------------------------------------
* = ignored for scoring

Summary:
Average mega ops / s: 90.9
```

## No-op allocator

This allocator always returns nullptr. This is a loose measure of the perftest
overhead.

```
---------------------------------------------------------------------------
| trace                           | correct? | mega ops / s | utilization |
---------------------------------------------------------------------------
|*traces/bdd-aa32.trace           |        Y |        581.6 |        inf% |
|*traces/bdd-aa4.trace            |        Y |       2842.5 |        inf% |
|*traces/bdd-ma4.trace            |        Y |       1118.8 |        inf% |
|*traces/bdd-nq7.trace            |        Y |        872.2 |        inf% |
|*traces/cbit-abs.trace           |        Y |       1071.8 |        inf% |
|*traces/cbit-parity.trace        |        Y |        656.8 |        inf% |
|*traces/cbit-satadd.trace        |        Y |        674.0 |        inf% |
|*traces/cbit-xyz.trace           |        Y |        720.5 |        inf% |
| traces/firefox.trace            |        Y |        410.6 |        inf% |
| traces/four-in-a-row.trace      |        Y |        429.6 |        inf% |
| traces/grep.trace               |        Y |        773.1 |        inf% |
| traces/haskell-web-server.trace |        Y |       1575.7 |        inf% |
| traces/mc_server.trace          |        Y |        434.8 |        inf% |
| traces/mc_server_large.trace    |        Y |        419.7 |        inf% |
| traces/mc_server_small.trace    |        Y |        969.4 |        inf% |
|*traces/ngram-fox1.trace         |        Y |       2943.7 |        inf% |
|*traces/ngram-gulliver1.trace    |        Y |        912.3 |        inf% |
|*traces/ngram-gulliver2.trace    |        Y |        663.8 |        inf% |
|*traces/ngram-moby1.trace        |        Y |        742.4 |        inf% |
|*traces/ngram-shake1.trace       |        Y |        730.1 |        inf% |
| traces/onoro.trace              |        Y |        655.7 |        inf% |
| traces/onoro-cc.trace           |        Y |        473.2 |        inf% |
| traces/py-catan-ai.trace        |        Y |        613.3 |        inf% |
| traces/py-euler-nayuki.trace    |        Y |        463.2 |        inf% |
| traces/scp.trace                |        Y |       1173.3 |        inf% |
|*traces/server.trace             |        Y |       2535.4 |        inf% |
|*traces/simple.trace             |        Y |       4875.5 |        inf% |
|*traces/simple_calloc.trace      |        Y |       3670.9 |        inf% |
|*traces/simple_realloc.trace     |        Y |       2192.7 |        inf% |
| traces/solitaire.trace          |        Y |        433.1 |        inf% |
| traces/ssh.trace                |        Y |       2439.6 |        inf% |
|*traces/syn-array.trace          |        Y |        732.5 |        inf% |
|*traces/syn-array-short.trace    |        Y |       2892.7 |        inf% |
|*traces/syn-mix.trace            |        Y |        753.9 |        inf% |
|*traces/syn-mix-realloc.trace    |        Y |       3296.4 |        inf% |
|*traces/syn-mix-short.trace      |        Y |       2730.3 |        inf% |
|*traces/syn-string.trace         |        Y |        754.4 |        inf% |
|*traces/syn-string-short.trace   |        Y |       2801.6 |        inf% |
|*traces/syn-struct.trace         |        Y |        750.1 |        inf% |
|*traces/syn-struct-short.trace   |        Y |       2893.0 |        inf% |
|*traces/test.trace               |        Y |       2722.0 |        inf% |
|*traces/test-zero.trace          |        Y |       4915.1 |        inf% |
| traces/vim.trace                |        Y |        440.6 |        inf% |
| traces/vlc.trace                |        Y |        415.8 |        inf% |
---------------------------------------------------------------------------
=======
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
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
* = ignored for scoring

Summary:
All correct? Y
<<<<<<< HEAD
Average mega ops / s: 638.4
=======
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
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
```
