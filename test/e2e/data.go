package main

type TicMode int64

const (
	TIC_MODE_HISTORIQUE TicMode = iota
	TIC_MODE_STANDARD
)

type MQTTResult struct {
	// TODO
}

type TestStep struct {
	Sent     []string
	Expected []MQTTResult
}

type TestCase struct {
	Name  string
	Mode  TicMode
	Steps []TestStep
}

var testCases []TestCase = []TestCase{
	{
		Name: "historique_simple",
		Mode: TIC_MODE_HISTORIQUE,
		Steps: []TestStep{
			{
				// LibTeleinfo explicitely discards the first frame
				Sent: []string{},
			},
			{
				Sent: []string{
					"MOTDETAT 000000 B",
					"PPOT 00 #",
					"OPTARIF HC.. <",
					"ISOUSC 25 =",
					"HCHC 015558379 1",
					"HCHP 011651340 (",
					"PTEC HP..  ",
					"IINST1 001 I",
					"IINST2 001 J",
					"IINST3 000 J",
					"IMAX1 060 6",
					"IMAX2 060 7",
					"IMAX3 060 8",
					"PMAX 08611 6",
					"PAPP 00540 *",
					"HHPHC A ,",
				},
			},
			{
				Sent: []string{
					"MOTDETAT 000000 B",
					"PPOT 00 #",
					"OPTARIF HC.. <",
					"ISOUSC 25 =",
					"HCHC 015558379 1",
					"HCHP 011651341 )",
					"PTEC HP..  ",
					"IINST1 001 I",
					"IINST2 009 R",
					"IINST3 000 J",
					"IMAX1 060 6",
					"IMAX2 060 7",
					"IMAX3 060 8",
					"PMAX 08611 6",
					"PAPP 02420 )",
					"HHPHC A ,",
				},
			},
			{
				Sent: []string{
					"MOTDETAT 000000 B",
					"PPOT 00 #",
					"OPTARIF HC.. <",
					"ISOUSC 25 =",
					"HCHC 015558379 1",
					"HCHP 011651343 +",
					"PTEC HP..  ",
					"IINST1 001 I",
					"IINST2 006 O",
					"IINST3 000 J",
					"IMAX1 060 6",
					"IMAX2 060 7",
					"IMAX3 060 8",
					"PMAX 08611 6",
					"PAPP 01690 1",
					"HHPHC A ,",
				},
			},
		},
	},
	{
		Name: "historique_adps_tri",
		Mode: TIC_MODE_HISTORIQUE,
		Steps: []TestStep{
			{
				// LibTeleinfo explicitely discards the first frame
				Sent: []string{},
			},
			{
				Sent: []string{
					"MOTDETAT 000000 B",
					"PPOT 00 #",
					"OPTARIF HC.. <",
					"ISOUSC 25 =",
					"HCHC 015558379 1",
					"HCHP 011651340 (",
					"PTEC HP..  ",
					"IINST1 001 I",
					"IINST2 001 J",
					"IINST3 000 J",
					"IMAX1 060 6",
					"IMAX2 060 7",
					"IMAX3 060 8",
					"PMAX 08611 6",
					"PAPP 00540 *",
					"HHPHC A ,",
				},
			},
			{
				Sent: []string{
					"ADCO 123456789012 G",
					"ADIR1 001 \"",
					"IINST1 001 I",
					"IINST2 009 R",
					"IINST3 000 J",
				},
			},
			{
				Sent: []string{
					"ADCO 123456789012 G",
					"ADIR1 001 \"",
					"IINST1 001 I",
					"IINST2 009 R",
					"IINST3 000 J",
				},
			},
			{
				Sent: []string{
					"ADCO 123456789012 G",
					"ADIR1 001 \"",
					"IINST1 001 I",
					"IINST2 009 R",
					"IINST3 000 J",
				},
			},
			{
				Sent: []string{
					"MOTDETAT 000000 B",
					"PPOT 00 #",
					"OPTARIF HC.. <",
					"ISOUSC 25 =",
					"HCHC 015558379 1",
					"HCHP 011651343 +",
					"PTEC HP..  ",
					"IINST1 001 I",
					"IINST2 006 O",
					"IINST3 000 J",
					"IMAX1 060 6",
					"IMAX2 060 7",
					"IMAX3 060 8",
					"PMAX 08611 6",
					"PAPP 01690 1",
					"HHPHC A ,",
				},
			},
		},
	},
}
