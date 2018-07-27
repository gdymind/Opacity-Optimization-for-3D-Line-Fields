# Opacity-Optimization-for-3D-Line-Fields

## Some Attentions or Potential problems/bugs

1. `fragmentNum = 7000000;': the maximum number can be more
2. compute matrix G with line length

## To Do List
1. Adding Lighting
2. Add OIT
3. Add Multisample
4. Add depth cue methods

## Thoughts:

2. H是二维的，计算量太大，能不能想办法降，如每个segment只保留一个重要程度？
3. H的计算过程中要考虑(i+1)与j、(j+1)的情况
4. 最重要的还是怎样定义G

