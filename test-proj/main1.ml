class virtual ['hrRep] employee = object
  method virtual receiveEvaluation : 'hrRep -> unit
  method virtual getName : string
end

let () = ()
let () = List.iter (fun _ -> ())
let x = 1

class ['a] accountant name =
object(self)
  inherit ['a] employee
  val name = name
  method receiveEvaluation rep = rep#visitAccountant self
  method getName = "A " ^ name
end

class ['a] salesman name =
object(self)
  inherit ['a] employee
  val name = name
  method receiveEvaluation rep = rep#visitSalesman self
  method getName = "S " ^ name
end

class virtual hrRep =
object
  method virtual visitAccountant : hrRep accountant -> unit
  method virtual visitSalesman : hrRep salesman -> unit
end

class lowerLevelHRRep =
object
  inherit hrRep
  method visitAccountant a = print_endline ("Visiting accountant " ^ a#getName)
  method visitSalesman s = print_endline ("Visiting salesman " ^ s#getName)
end

let bob = new salesman "Bob"
let mary = new accountant "Mary"
let sue = new salesman "Sue"
let h = new lowerLevelHRRep
let _ = bob#receiveEvaluation h
let _ = mary#receiveEvaluation h
let _ = sue#receiveEvaluation h


