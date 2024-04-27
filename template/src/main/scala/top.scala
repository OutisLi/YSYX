package scala.top

import chisel3._
import Mux2_1._

class top extends Module {
  val io = IO(new Bundle {
    val a   = Input(UInt(1.W))
    val b   = Input(UInt(1.W))
    val sel = Input(Bool())
    val y   = Output(UInt(1.W))
  })

  val mux2_1 = Module(new Mux2_1)
  mux2_1.io.a   := io.a
  mux2_1.io.b   := io.b
  mux2_1.io.sel := io.sel
  io.y          := mux2_1.io.y
}
