# pg_int_split

This is a PostgreSQL extension that provides a window function to subdivide integer sums into weighted integer portions using the largest remainder method.

A common use is for splitting monetary amounts according to a given set of share weights, where the subdivided amounts must sum to the exact total amount but pennies cannot be subdivided. A naïve attempt at splitting by multiplying the total by each fractional proportion and rounding will not always yield a set of amounts that sum to the total.

The largest remainder method computes each proportionate share of the total, rounding down and remembering each share's remainder. Then it distributes the total remainder *R*, which will always be an integer, by adding 1 to each of the *R* shares having the greatest remainders. This ensures that the “most deserving” shares are the ones that get rounded up.

This extension avoids using floating-point arithmetic. Portions are calculated using (32×32→64)÷32→32 and (64×64→128)÷64→64 integer “muldiv” operations. Unfortunately, because C lacks a primitive for such operations, they are implemented in assembly language (currently for x64 only).

**<code>int_split(<em>total</em> integer, <em>weight</em> integer)</code> → `integer`**  
**<code>int_split(<em>total</em> bigint, <em>weight</em> bigint)</code> → `bigint`**  
  ⇒ Subdivides *`total`* into a portion having the specified *`weight`* among all portions in the window.

## Examples

Say we want to subdivide 100 widgets into three integer portions having relative weights 2, 3, and 4. The naïve approach would multiply 100 by 2⁄9, 3⁄9, and 4⁄9, and round each product to the nearest integer. However, that gives us portions of 22, 33, and 44, which sum to only 99, not 100. To get a correct split, we can use the largest remainder method by calling `int_split`.

```PLpgSQL
=> SELECT id, weight,
        100 * weight::float / sum(weight) OVER () AS exact,
        (100 * weight::float / sum(weight) OVER ())::int AS naive,
        int_split(100, weight) OVER ()
    FROM (VALUES (1, 2), (2, 3), (3, 4)) _ (id, weight);
```
```
 id | weight |       exact        | naive | int_split 
----+--------+--------------------+-------+-----------
  1 |      2 |  22.22222222222222 |    22 |        22
  2 |      3 | 33.333333333333336 |    33 |        33
  3 |      4 |  44.44444444444444 |    44 |        45
(3 rows)
```

The `int_split` function chooses to give the “extra” unit to the portion for ID 3 because that portion's exact amount is closest to being rounded up.

Because `int_split` is a window function, you can also specify a windowing partition. (It makes no sense to specify a sort order.)

```PLpgSQL
=> SELECT expense_id, total, payer_id, weight,
        total * (weight::float / sum(weight) OVER (expense)) AS exact,
        int_split(total, weight) OVER (expense)
    FROM (VALUES (1, 5), (2, 3), (3, 4))
            payers (payer_id, weight),
        (VALUES (1, 6277), (2, 10924), (3, 8651))
            expenses (expense_id, total)
    WINDOW expense AS (PARTITION BY expense_id)
    ORDER BY expense_id, payer_id;
```
```
 expense_id | total | payer_id | weight |       exact        | int_split 
------------+-------+----------+--------+--------------------+-----------
          1 |  6277 |        1 |      5 | 2615.4166666666665 |      2616
          1 |  6277 |        2 |      3 |            1569.25 |      1569
          1 |  6277 |        3 |      4 | 2092.3333333333335 |      2092
          2 | 10924 |        1 |      5 |  4551.666666666667 |      4552
          2 | 10924 |        2 |      3 |               2731 |      2731
          2 | 10924 |        3 |      4 | 3641.3333333333335 |      3641
          3 |  8651 |        1 |      5 | 3604.5833333333335 |      3604
          3 |  8651 |        2 |      3 |            2162.75 |      2163
          3 |  8651 |        3 |      4 | 2883.6666666666665 |      2884
(9 rows)
```

This example demonstrates that portions can be rounded up even if their exact fractional part is less than 0.5, and portions can be rounded down even if their exact fractional part is greater than 0.5. What determines whether a portion will be rounded up or down is how that portion's fractional part ranks among the fractional parts of all portions within the same window.

## Installation

The only prerequisites are PostgreSQL and a C compiler toolchain. Then it's just `make` to compile the module and `make install` to install the extension in your PostgreSQL extensions directory.

Once the extension is installed on your system, you can install it into your database by executing:
```PLpgSQL
=> CREATE EXTENSION pg_int_split WITH SCHEMA public;
```
