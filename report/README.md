# hw5 report

|||
|-:|:-|
|Name|陳吉遠|
|ID|109550028|

## How much time did you spend on this project

> 16 hours

## Project overview

這次作業要把我們自己定義的 P language 轉成 RISC-V，實作方法是寫一個叫做 Generator 的 Visitor，他 visit 到每個 Node 的時候會 dump 出對應的程式碼，接下來搭配 HW4 所實作的 Symbol Table，我們可以知道每一個 Symbol 所指何物，可以很方便的換算每個 local 變數的地址。

然後由於 HW4 已經進行了 Semantic 分析，因此我們可以毫無後顧之憂的假設程式碼都可以被正確執行，而不做特別的例外處理。

## What is the hardest you think in this project

最困難的就是生出對應的組合語言。

我對 RISC-V 並不算熟悉，對於組語的瞭解停留在多年以前修的計算機組織，所以在思考每一步驟要怎麼做的時候，大腦一直陷入混亂；但除此之外都不算太困難。

我覺得假設是熟悉組合語言與整體架構的人來做，這次作業應該可以很容易的被完成。

## Feedback to T.A.s

讚歎助教提供的 Symbol Table 的 template，我 hw4 寫的 symbol table 是一次性的，如果助教沒提供，我就被迫要把 semantic check 和 asm generator 寫在一起了。
Spec 的引導很清楚，基本上整體沒遇到什麼大問題！

