package scala.Mux2_1

import chisel3._

class Mux2_1 extends Module {
  val io = IO(new Bundle {
    val a   = Input(UInt(1.W))
    val b   = Input(UInt(1.W))
    val sel = Input(Bool())
    val y   = Output(UInt(1.W))
  })

  io.y := Mux(io.sel, io.b, io.a)
}
