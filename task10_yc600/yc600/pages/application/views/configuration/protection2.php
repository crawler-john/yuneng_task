<!-- 设置结果显示框 -->
<div class="alert alert-success" id="result"></div>

<!--13项交流保护参数设置表单 -->
<form class="form-horizontal" id="defaultForm" method="ajax">
  <fieldset>
    <legend><?php echo $this->lang->line('protection_set')?></legend>

    <div class="form-group">    
      <label for="inputdata0" class="col-sm-5 control-label"><?php echo $this->lang->line('protection_select_inverter')?></label>
      <div class="col-sm-4">
        <select name='inverter' class="form-control" id="inputdata0">";
          <option value="all"><?php echo $this->lang->line('protection_select_inverter_all')?></option>
          <?php
            foreach ($ids as $key => $value) {
                echo "<option value=\"$value\">".$value."</option>";
            }
          ?>
        </select>
      </div>
    </div>

    <div class="form-group">
      <label for="inputdata1" class="col-sm-5 control-label"><?php echo $this->lang->line('protection_under_voltage_fast')?></label>
      <div class="col-sm-4">
        <div class="input-group">
          <input type="text" name="under_voltage_fast" class="form-control" id="inputdata1" placeholder="<?php echo $under_voltage_fast;?>">
          <span class="input-group-addon"><div class="unit">V</div></span>
        </div>
      </div>
    </div>

    <div class="form-group">
      <label for="inputdata2" class="col-sm-5 control-label"><?php echo $this->lang->line('protection_over_voltage_fast')?></label>
      <div class="col-sm-4">
        <div class="input-group">
          <input type="text" name="over_voltage_fast" class="form-control" id="inputdata2" placeholder="<?php echo $over_voltage_fast;?>">
          <span class="input-group-addon"><div class="unit">V</div></span>
        </div>
      </div>
    </div>

    <div class="form-group">
      <label for="inputdata3" class="col-sm-5 control-label"><?php echo $this->lang->line('protection_under_voltage_slow')?></label>
      <div class="col-sm-4">
        <div class="input-group">
          <input type="text" name="under_voltage_slow" class="form-control" id="inputdata3" placeholder="<?php echo $under_voltage_slow;?>">
          <span class="input-group-addon"><div class="unit">V</div></span>
        </div>
      </div>
    </div>

    <div class="form-group">
      <label for="inputdata4" class="col-sm-5 control-label"><?php echo $this->lang->line('protection_over_voltage_slow')?></label>
      <div class="col-sm-4">
        <div class="input-group">
          <input type="text" name="over_voltage_slow" class="form-control" id="inputdata4" placeholder="<?php echo $over_voltage_slow;?>">
          <span class="input-group-addon"><div class="unit">V</div></span>
        </div>
      </div>
    </div>

    <div class="form-group">
      <label for="inputdata5" class="col-sm-5 control-label"><?php echo $this->lang->line('protection_under_frequency_fast')?></label>
      <div class="col-sm-4">
        <div class="input-group">
          <input type="text" name="under_frequency_fast" class="form-control" id="inputdata5" placeholder="<?php echo $under_frequency_fast;?>">
          <span class="input-group-addon"><div class="unit">Hz</div></span>
        </div>
      </div>
    </div>

    <div class="form-group">
      <label for="inputdata6" class="col-sm-5 control-label"><?php echo $this->lang->line('protection_over_frequency_fast')?></label>
      <div class="col-sm-4">
        <div class="input-group">
          <input type="text" name="over_frequency_fast" class="form-control" id="inputdata6" placeholder="<?php echo $over_frequency_fast;?>">
          <span class="input-group-addon"><div class="unit">Hz</div></span>
        </div>
      </div>
    </div>

    <div class="form-group">
      <label for="inputdata7" class="col-sm-5 control-label"><?php echo $this->lang->line('protection_under_frequency_slow')?></label>
      <div class="col-sm-4">
        <div class="input-group">
          <input type="text" name="under_frequency_slow" class="form-control" id="inputdata7" placeholder="<?php echo $under_frequency_slow;?>">
          <span class="input-group-addon"><div class="unit">Hz</div></span>
        </div>
      </div>
    </div>

    <div class="form-group">
      <label for="inputdata8" class="col-sm-5 control-label"><?php echo $this->lang->line('protection_over_frequency_slow')?></label>
      <div class="col-sm-4">
        <div class="input-group">
          <input type="text" name="over_frequency_slow" class="form-control" id="inputdata8" placeholder="<?php echo $over_frequency_slow;?>">
          <span class="input-group-addon"><div class="unit">Hz</div></span>
        </div>
      </div>
    </div>

    <div class="form-group">
      <label for="inputdata9" class="col-sm-5 control-label"><?php echo $this->lang->line('protection_voltage_triptime_fast')?></label>
      <div class="col-sm-4">
        <div class="input-group">
          <input type="text" name="voltage_triptime_fast" class="form-control" id="inputdata9" placeholder="<?php echo $voltage_triptime_fast;?>">
          <span class="input-group-addon"><div class="unit">s</div></span>
        </div>
      </div>
    </div>

    <div class="form-group">
      <label for="inputdata10" class="col-sm-5 control-label"><?php echo $this->lang->line('protection_voltage_triptime_slow')?></label>
      <div class="col-sm-4">
        <div class="input-group">
          <input type="text" name="voltage_triptime_slow" class="form-control" id="inputdata10" placeholder="<?php echo $voltage_triptime_slow;?>">
          <span class="input-group-addon"><div class="unit">s</div></span>
        </div>
      </div>
    </div>

    <div class="form-group">
      <label for="inputdata11" class="col-sm-5 control-label"><?php echo $this->lang->line('protection_frequency_triptime_fast')?></label>
      <div class="col-sm-4">
        <div class="input-group">
          <input type="text" name="frequency_triptime_fast" class="form-control" id="inputdata11" placeholder="<?php echo $frequency_triptime_fast;?>">
          <span class="input-group-addon"><div class="unit">s</div></span>
        </div>
      </div>
    </div>

    <div class="form-group">
      <label for="inputdata12" class="col-sm-5 control-label"><?php echo $this->lang->line('protection_frequency_triptime_slow')?></label>
      <div class="col-sm-4">
        <div class="input-group">
          <input type="text" name="frequency_triptime_slow" class="form-control" id="inputdata12" placeholder="<?php echo $frequency_triptime_slow;?>">
          <span class="input-group-addon"><div class="unit">s</div></span>
        </div>
      </div>
    </div>

    <div class="form-group">
      <label for="inputdata13" class="col-sm-5 control-label"><?php echo $this->lang->line('protection_grid_recovery_time')?></label>
      <div class="col-sm-4">
        <div class="input-group">
          <input type="text" name="grid_recovery_time" class="form-control" id="inputdata13" placeholder="<?php echo $grid_recovery_time;?>">
          <span class="input-group-addon"><div class="unit">s</div></span>
        </div>      
      </div>
    </div>
  </fieldset>
  
  <div class="form-group">
    <div class="col-sm-offset-5 col-sm-2">
      <button class="btn btn-primary btn-sm" id="button_save" type="submit"><?php echo $this->lang->line('button_save')?></button>
    </div>
  </div>
</form>

<!-- 13项交流保护参数实际值显示表格 -->
<fieldset>
  <legend><?php echo $this->lang->line('protection_actual_value')?>
    <div class="btn-group">
        <button class="btn btn-default btn-xs" id="refresh" type="button"><?php echo $this->lang->line('protection_read_parameters')?></button>
    </div>
  </legend>

  <div class="table-responsive">
    <table class="table table-condensed table-striped table-hover table-bordered">
      <thead>
        <tr>
          <?php
            if(!empty($parameters))
            {
              echo "<th>".$this->lang->line('device_id')."</th>";
              echo "<th>".$this->lang->line('protection_under_voltage_fast')."</th>";
              echo "<th>".$this->lang->line('protection_over_voltage_fast')."</th>";
              echo "<th>".$this->lang->line('protection_under_voltage_slow')."</th>";
              echo "<th>".$this->lang->line('protection_over_voltage_slow')."</th>";
              echo "<th>".$this->lang->line('protection_under_frequency_fast')."</th>";
              echo "<th>".$this->lang->line('protection_over_frequency_fast')."</th>";
              echo "<th>".$this->lang->line('protection_under_frequency_slow')."</th>";
              echo "<th>".$this->lang->line('protection_over_frequency_slow')."</th>";
              echo "<th>".$this->lang->line('protection_voltage_triptime_fast')."</th>";
              echo "<th>".$this->lang->line('protection_voltage_triptime_slow')."</th>";
              echo "<th>".$this->lang->line('protection_frequency_triptime_fast')."</th>";
              echo "<th>".$this->lang->line('protection_frequency_triptime_slow')."</th>";
              echo "<th>".$this->lang->line('protection_grid_recovery_time')."</th>";
            }
          ?>         
        </tr>
      </thead>
      <tbody>
      <?php
          foreach ($parameters as $value) 
          {
              echo "<tr>";
              echo "<td>".$value['inverter_id']."</td>";
              echo "<td>".$value['under_voltage_fast']."</td>";
              echo "<td>".$value['over_voltage_fast']."</td>";
              echo "<td>".$value['under_voltage_slow']."</td>";
              echo "<td>".$value['over_voltage_slow']."</td>";
              echo "<td>".$value['under_frequency_fast']."</td>";
              echo "<td>".$value['over_frequency_fast']."</td>";
              echo "<td>".$value['under_frequency_slow']."</td>";
              echo "<td>".$value['over_frequency_slow']."</td>";
              echo "<td>".$value['voltage_triptime_fast']."</td>";
              echo "<td>".$value['voltage_triptime_slow']."</td>";
              echo "<td>".$value['frequency_triptime_fast']."</td>";
              echo "<td>".$value['frequency_triptime_slow']."</td>";
              echo "<td>".$value['grid_recovery_time']."</td>";       
              echo "</tr>";
          }
      ?>
      </tbody>
    </table>
  </div>
</fieldset>

<script>
$(document).ready(function() 
{
	//表单验证
    $('#defaultForm').bootstrapValidator({
        message: 'This value is not valid',
        fields: {
            under_voltage_fast: {
                validators: {
                    stringLength: {
                        max: 16,
                        message: 'The input string must be less than 16 characters long'
                    },
                    regexp: {
                        regexp: /^\d{0,3}$/,
                        message: '<?php echo $this->lang->line('validform_under_voltage_fast')?>'
                    }
                }
            },
            over_voltage_fast: {
                validators: {
                    stringLength: {
                        max: 16,
                        message: 'The input string must be less than 16 characters long'
                    },
                    regexp: {
                        regexp: /^\d{0,3}$/,
                        message: '<?php echo $this->lang->line('validform_over_voltage_fast')?>'
                    }
                }
            },
            under_voltage_slow: {
                validators: {
                    stringLength: {
                        max: 16,
                        message: 'The input string must be less than 16 characters long'
                    },
                    regexp: {
                        regexp: /^\d{0,3}$/,
                        message: '<?php echo $this->lang->line('validform_under_voltage_slow')?>'
                    }
                }
            },
            over_voltage_slow: {
                validators: {
                    stringLength: {
                        max: 16,
                        message: 'The input string must be less than 16 characters long'
                    },
                    regexp: {
                        regexp: /^\d{0,3}$/,
                        message: '<?php echo $this->lang->line('validform_over_voltage_slow')?>'
                    }
                }
            },
            under_frequency_fast: {
                validators: {
                    stringLength: {
                        max: 16,
                        message: 'The input string must be less than 16 characters long'
                    },
                    regexp: {
                        regexp: /^0*\d{1,2}(\.\d)?$/,
                        message: '<?php echo $this->lang->line('validform_under_frequency_fast')?>'
                    }
                }
            },
            over_frequency_fast: {
                validators: {
                    stringLength: {
                        max: 16,
                        message: 'The input string must be less than 16 characters long'
                    },
                    regexp: {
                        regexp: /^0*\d{1,2}(\.\d)?$/,
                        message: '<?php echo $this->lang->line('validform_over_frequency_fast')?>'
                    }
                }
            },
            under_frequency_slow: {
                validators: {
                    stringLength: {
                        max: 16,
                        message: 'The input string must be less than 16 characters long'
                    },
                    regexp: {
                        regexp: /^0*\d{1,2}(\.\d)?$/,
                        message: '<?php echo $this->lang->line('validform_under_frequency_slow')?>'
                    }
                }
            },
            over_frequency_slow: {
                validators: {
                    stringLength: {
                        max: 16,
                        message: 'The input string must be less than 16 characters long'
                    },
                    regexp: {
                        regexp: /^0*\d{1,2}(\.\d)?$/,
                        message: '<?php echo $this->lang->line('validform_over_frequency_slow')?>'
                    }
                }
            },
            voltage_triptime_fast: {
                validators: {
                    stringLength: {
                        max: 16,
                        message: 'The input string must be less than 16 characters long'
                    },
                    regexp: {
                        regexp: /^0*\d{1,3}(\.\d{1,2})?$/,
                        message: '<?php echo $this->lang->line('validform_voltage_triptime_fast')?>'
                    }
                }
            },
            voltage_triptime_slow: {
                validators: {
                    stringLength: {
                        max: 16,
                        message: 'The input string must be less than 16 characters long'
                    },
                    regexp: {
                        regexp: /^0*\d{1,3}(\.\d{1,2})?$/,
                        message: '<?php echo $this->lang->line('validform_voltage_triptime_slow')?>'
                    }
                }
            },
            frequency_triptime_fast: {
                validators: {
                    stringLength: {
                        max: 16,
                        message: 'The input string must be less than 16 characters long'
                    },
                    regexp: {
                        regexp: /^0*\d{1,3}(\.\d{1,2})?$/,
                        message: '<?php echo $this->lang->line('validform_frequency_triptime_fast')?>'
                    }
                }
            },
            frequency_triptime_slow: {
                validators: {
                    stringLength: {
                        max: 16,
                        message: 'The input string must be less than 16 characters long'
                    },
                    regexp: {
                        regexp: /^0*\d{1,3}(\.\d{1,2})?$/,
                        message: '<?php echo $this->lang->line('validform_frequency_triptime_slow')?>'
                    }
                }
            },
            grid_recovery_time: {
                validators: {
                    stringLength: {
                        max: 16,
                        message: 'The input string must be less than 16 characters long'
                    },
                    regexp: {
                        regexp: /^\d{0,5}$/,
                        message: '<?php echo $this->lang->line('validform_grid_recovery_time')?>'
                    }
                }
            }
        }
    })
    .on('success.form.bv', function(e) {
        //防止默认表单提交，采用ajax提交
        e.preventDefault();
    });

    //设置表单处理
    $("#button_save").click(function(){
    	$("#result").hide();    	
	    $.ajax({
    		url : "<?php echo base_url('index.php/configuration/set_protection2');?>",
    		type : "post",
            dataType : "json",
    		data: "inverter=" + $("#inputdata0").val()
    			  + "&under_voltage_fast=" + $("#inputdata1").val() 
    		      + "&over_voltage_fast=" + $("#inputdata2").val() 
    		      + "&under_voltage_slow=" + $("#inputdata3").val()
    		      + "&over_voltage_slow=" + $("#inputdata4").val() 
    		      + "&under_frequency_fast=" + $("#inputdata5").val() 
    		      + "&over_frequency_fast=" + $("#inputdata6").val() 
    		      + "&under_frequency_slow=" + $("#inputdata7").val() 
    		      + "&over_frequency_slow=" + $("#inputdata8").val() 
    		      + "&voltage_triptime_fast=" + $("#inputdata9").val() 
    		      + "&voltage_triptime_slow=" + $("#inputdata10").val() 
    		      + "&frequency_triptime_fast=" + $("#inputdata11").val() 
    		      + "&frequency_triptime_slow=" + $("#inputdata12").val() 
    		      + "&grid_recovery_time=" + $("#inputdata13").val(),
            success : function(Results){
            	$("#result").text(Results.message);
                if(Results.value == 0){
                    $("#result").removeClass().addClass("alert alert-success");
                    setTimeout('$("#result").fadeToggle("slow")', 3000);
                }
                else{
                    $("#result").removeClass().addClass("alert alert-warning");
                }
                $("#result").fadeToggle("slow");
                window.scrollTo(0,0);//页面置顶
                $("#button_save").attr('disabled',false);
            },
            error : function() { alert("Error"); }
        })
    });

    //读取逆变器保护参数
    $("#refresh").click(function(){
    	$("#result").hide();    	
	    $.ajax({
    		url : "<?php echo base_url('index.php/configuration/read_inverter_parameters');?>",
    		type : "post",
            dataType : "json",
    		data: "read_inverter_parameters",
    		success : function(Results){
            	$("#result").text(Results.message);
                if(Results.value == 0){
                    $("#result").removeClass().addClass("alert alert-success");
                    setTimeout('$("#result").fadeToggle("slow")', 3000);
                }
                else{
                    $("#result").removeClass().addClass("alert alert-warning");
                }
                $("#result").fadeToggle("slow");
                window.scrollTo(0,0);//页面置顶 
            },
            error : function() { alert("Error"); }
        })
    });  
});
</script>