<!-- 设置结果显示框 -->
<div class="alert alert-success" id="result"></div>

<!-- 加密信息设置表格 -->
<!-- 表单 -->
<form class="form-horizontal" id="defaultForm" method="ajax">
  <?php 
  if($flagen){
	echo "<!--";
     }
  ?>
<div class="form-group">    
  <label for="inputdata" class="col-sm-7 control-label"><font size = "4"><?php echo $this->lang->line('encrypted')?></font></label>
</div>  
  	<?php 
  if($flagen){
      echo "-->";
    }
    ?>  
    
  <?php 
  if(!$flagen){
	echo "<!--";
     }
  ?>
<div class="form-group">    
  <label for="inputdata1" class="col-sm-3 control-label"><?php echo $this->lang->line('psword')?></label>
  <div class="col-sm-4">
    <div class="input-group">     
    <input type="text" name="password" class="form-control" id="password">
    </div> 
   </div>
	<div class="col-sm-1">
  	  <button class="btn btn-primary btn-sm" id="button_ok" type="submit"><?php echo $this->lang->line('button_ok')?></button>
	</div> 
</div>
  	<?php 
  if(!$flagen){
      echo "-->";
    }
    ?> 
</form>

<script>
$(document).ready(function() 
		{	
	//表单验证
    $('#defaultForm').bootstrapValidator({
        message: 'This value is not valid',
        fields: {
        	password: {
                validators: {
                    notEmpty: {
                        message: '<?php echo $this->lang->line('validform_null_psw')?>'
                    },
 //                   stringLength: {
 //                       max: 1,
 //                       message: 'The_input_string_must_be_1'
 //                   },
                    regexp: {
                        regexp: /^[0-9a-zA-Z]{8}$/,
                        message: '<?php echo $this->lang->line('validform_psw')?>'
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
	$("#button_ok").click(function(){
	$("#result").hide(); 
    	$.ajax({
			url : "<?php echo base_url('index.php/configuration/check_encrypt');?>",
			type : "post",
        	dataType : "json",
			data: "password=" + $("#password").val(),			  
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
            	setTimeout("location.reload();",60000);//刷新页面
			},
	    		error : function() { alert("Error"); }
    	})        
	});    
  });	    
</script>