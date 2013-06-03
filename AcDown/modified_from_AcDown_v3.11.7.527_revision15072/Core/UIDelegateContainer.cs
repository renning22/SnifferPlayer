﻿using System;
using Kaedei.AcDown.Interface;

namespace Kaedei.AcDown.Core
{
	/// <summary>
	/// UI委托包装类
	/// </summary>
	public class UIDelegateContainer : DelegateContainer
	{
		public UIDelegateContainer(AcTaskDelegate startDele,
							 AcTaskDelegate newPartDele,
							 AcTaskDelegate refreshDele,
							 AcTaskDelegate tipTextDele,
							 AcTaskDelegate finishDele,
							 AcTaskDelegate errorDele,
							 AcTaskDelegate newTaskDele)
			: base(newPartDele, refreshDele, tipTextDele, newTaskDele)
		{
			Start += startDele;
			Finish += finishDele;
			Error += errorDele;
		}

		public AcTaskDelegate Start { get; set; }
		public AcTaskDelegate Finish { get; set; }
		public AcTaskDelegate Error { get; set; }

	}

	public class ParaFinish : DelegateParameter
	{
		public ParaFinish(TaskInfo task, bool isSuccess) { SourceTask = task; Successed = isSuccess; }
		public bool Successed { get; set; }
	}

	public class ParaError : DelegateParameter
	{
		public ParaError(TaskInfo task, Exception excp) { SourceTask = task; E = excp; }
		public Exception E { get; set; }
	}

	public class ParaStart : DelegateParameter
	{
		public ParaStart(TaskInfo task) { SourceTask = task; }
	}
}
